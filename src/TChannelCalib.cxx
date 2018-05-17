#include "TChannelCalib.hxx"

#include <TEvent.hxx>
#include <THandle.hxx>
#include <TRealDatum.hxx>
#include <TEventFolder.hxx>
#include <TCaptLog.hxx>
#include <TChannelId.hxx>
#include <TMCChannelId.hxx>
#include <HEPUnits.hxx>
#include <TRuntimeParameters.hxx>
#include <CaptGeomId.hxx>
#include <TChannelInfo.hxx>
#include <TTPCChannelId.hxx>

#include <TTPC_Bad_Channel_Table.hxx>
#include <TTPC_Channel_Calib_Table.hxx>

#include <TResultSetHandle.hxx>
#include <DatabaseUtils.hxx>
#include <TDbi.hxx>

#define GET_CALIBRATION_STATUS

namespace {
    // This is slightly evil, but keep a "local" cache of the most recent
    // calibration coefficients.  This prevents exposing the cache to the
    // TChannelCalib users, and

    // A cache for the bad channel table.
    CP::TEventContext gTPCBadChannelContext;
    CP::TEventContext gMCBadChannelContext;
    typedef std::map<CP::TChannelId,int> TPCBadChannelMap;
    TPCBadChannelMap gTPCBadChannels;
    TPCBadChannelMap gMCBadChannels;
    void UpdateTPCBadChannels() {
        CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
        if (!ev) {
            gTPCBadChannels.clear();
            return;
        }
        CP::TEventContext context = ev->GetContext();
        if (context == gTPCBadChannelContext) {return;};
        gTPCBadChannelContext = context;
        gTPCBadChannels.clear();
        // Get the bad channel table.
	CP::TResultSetHandle<CP::TTPC_Bad_Channel_Table> chanTable(context);
        Int_t numChannels(chanTable.GetNumRows());

        for (int i = 0; i<numChannels; ++i) {
            const CP::TTPC_Bad_Channel_Table* chanRow = chanTable.GetRow(i);
            if (!chanRow) continue;
            CP::TChannelId chanId = chanRow->GetChannelId();
            gTPCBadChannels[chanId] = chanRow->GetChannelStatus();
        }
        
        CaptLog("Bad channel table update: " << gTPCBadChannelContext);
        
    }

    void UpdateMCBadChannels() {
        CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
        if (!ev) {
            gMCBadChannels.clear();
            return;
        }
        CP::TEventContext context = ev->GetContext();
	std::time_t t2 = std::time(0);
	context.SetTimeStamp(t2);
        if (context == gMCBadChannelContext) {return;};
        gMCBadChannelContext = context;
        gMCBadChannels.clear();
        // Get the bad channel table.
	CP::TResultSetHandle<CP::TTPC_Bad_Channel_Table> chanTable(context);
        Int_t numChannels(chanTable.GetNumRows());

        for (int i = 0; i<numChannels; ++i) {
            const CP::TTPC_Bad_Channel_Table* chanRow = chanTable.GetRow(i);
            if (!chanRow) continue;
	    //chanRow->PrintMC();
            CP::TChannelId chanId = chanRow->GetChannelMCId();
            gMCBadChannels[chanId] = chanRow->GetChannelStatus();
        }
        
        CaptLog("Bad channel table update: " << gMCBadChannelContext);
        
    }

    // A cache for the tpc pulse gain and shape calibration table
    CP::TEventContext gTPCChannelCalibContext;
    typedef std::map<CP::TChannelId,int> TPCChannelCalibMap;
    TPCChannelCalibMap gTPCChannelCalib;
    
}

CP::TChannelCalib::TChannelCalib() { }

CP::TChannelCalib::~TChannelCalib() { } 

bool CP::TChannelCalib::IsGoodChannel(CP::TChannelId id) {
    int status = GetChannelStatus(id);
    status &= ~(TTPC_Channel_Calib_Table::kLowGain
                |TTPC_Channel_Calib_Table::kHighGain
                |TTPC_Channel_Calib_Table::kBadPeak
		|TTPC_Channel_Calib_Table::kBadFit);
    if (status != 0) return false;
    return true;
}

bool CP::TChannelCalib::IsBipolarSignal(CP::TChannelId id) {
    if (id.IsMCChannel()) {
        TMCChannelId mc(id);

        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptWarn("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        switch (index) {
        case 1: case 2: return true;
        default: return false;
        }
    }

    /// Get the event context.
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    if (!ev) {
        CaptError("No event is loaded so context cannot be set.");
        throw EChannelCalibUnknownType();
    }
    CP::TEventContext context = ev->GetContext();

    /// Get the geometry id for the current wire.
    CP::TGeometryId geomId = CP::TChannelInfo::Get().GetGeometry(id);
    if (!geomId.IsValid()) {
        CP::TTPCChannelId tpcId(id);
        if (!tpcId.IsValid()) {
            CaptError("Not a TPC Channel");
        }
        if (tpcId.GetCrate()<2) return true;
        return false;
    }
    if (!CP::GeomId::Captain::IsWire(geomId)) {
        CaptError("Channel " << id << " is not a wire: " << geomId);
        return false;
    }

    // Check for special runs.  This should really be done with a table, but
    // for now, it is hardcoded.
    if (context.IsMiniCAPTAIN()
        && 4090 <= context.GetRun()
        && context.GetRun() < 6000) {
        // The X wires are disconnected for most of these runs, but return
        // "unipolar" anyway since it makes other studies easier.
        if (CP::GeomId::Captain::IsXWire(geomId)) return false;
        if (CP::GeomId::Captain::IsVWire(geomId)) return false;
        return true;
    }

    // Normally, the X wire is the collection, and the other wires (U, V) are
    // induction (i.e. bipolar).
    if (CP::GeomId::Captain::IsXWire(geomId)) return false;
    return true;

    CaptWarn("Unknown channel: " << id);
    throw EChannelCalibUnknownType();
    return false;
}

int CP::TChannelCalib::GetChannelStatus(CP::TChannelId id) {
    if (id.IsMCChannel()) {
	//return 0;
	TPCBadChannelMap::iterator val = gMCBadChannels.find(id);
	UpdateMCBadChannels();

	if (val != gMCBadChannels.end()) return val->second;
	else return 0;
    }
    else {
	UpdateTPCBadChannels();
	TPCBadChannelMap::iterator val = gTPCBadChannels.find(id);
	if (val != gTPCBadChannels.end()) return val->second;

#ifdef GET_CALIBRATION_STATUS
	// Get the status of the calibration fit for this channel.  This should
	// only be enabled after the calibration fitting routine has settled on a
	// good set of statis bits.
    
	CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
	if (!ev) {
	    CaptError("No event is loaded so context cannot be set.");
	    throw EChannelCalibUnknownType();
	}

	CP::TEventContext context = ev->GetContext();
	CP::TResultSetHandle<CP::TTPC_Channel_Calib_Table> table(context);
	const CP::TTPC_Channel_Calib_Table* row = table.GetRowByIndex(id.AsUInt());
	
	// Empty table, so all good.
	if (table.GetNumRows()<10) return 0;
	
	if (!row) return CP::TTPC_Channel_Calib_Table::kNoSignal;
    
	return row->GetChannelStatus();
#else
	return 0;
#endif
    }
}

double CP::TChannelCalib::GetGainConstant(CP::TChannelId id, int order) {
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    if (!ev) {
        CaptError("No event is loaded so context cannot be set.");
        throw EChannelCalibUnknownType();
    }
    CP::TEventContext context = ev->GetContext();
        
    if (id.IsMCChannel()) {
        TMCChannelId mc(id);

        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptWarn("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        if (order == 1) {
            // Get the gain
            CP::THandle<CP::TRealDatum> gainVect
                = ev->Get<CP::TRealDatum>("~/truth/elecSimple/gain");
            return (*gainVect)[index];
        }

        return 0.0;
    }

    CP::TResultSetHandle<CP::TTPC_Channel_Calib_Table> table(context);
    const CP::TTPC_Channel_Calib_Table* row = table.GetRowByIndex(id.AsUInt());

    if (!row) {
        CaptWarn("Unknown channel: " << id);
        if (order == 1) return (14.0*unit::mV/unit::fC);
        return 0.0;
    }

    if (order == 1) return row->GetASICGain()*unit::mV/unit::fC;
    return 0.0;
}

double CP::TChannelCalib::GetAveragePulseShapePeakTime(CP::TChannelId id,
                                                       int order) {
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    if (!ev) {
        CaptError("No event is loaded so context cannot be set.");
        throw EChannelCalibUnknownType();
    }
    CP::TEventContext context = ev->GetContext();

    if (id.IsMCChannel()) return GetPulseShapePeakTime(id,order);

    CP::TResultSetHandle<CP::TTPC_Channel_Calib_Table> table(context);
    
    Int_t numChannels(table.GetNumRows());

    double peakTime = 0.0;
    double count = 0.0;
    for (int i = 0; i<numChannels; ++i) {
        const CP::TTPC_Channel_Calib_Table* row = table.GetRow(i);
        peakTime += row->GetASICPeakTime()*unit::ns;
        count += 1.0;
    }
    peakTime /= count;
    
    return peakTime;
}

double CP::TChannelCalib::GetPulseShapePeakTime(CP::TChannelId id, int order) {
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    if (!ev) {
        CaptError("No event is loaded so context cannot be set.");
        throw EChannelCalibUnknownType();
    }
    CP::TEventContext context = ev->GetContext();

    if (id.IsMCChannel()) {
        TMCChannelId mc(id);
        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptWarn("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        // Get the peaking time.
        CP::THandle<CP::TRealDatum> shapeVect
            = ev->Get<CP::TRealDatum>("~/truth/elecSimple/shape");
        double peakingTime = (*shapeVect)[index];
        return peakingTime;
    }

    CP::TResultSetHandle<CP::TTPC_Channel_Calib_Table> table(context);
    const CP::TTPC_Channel_Calib_Table* row = table.GetRowByIndex(id.AsUInt());

    if (!row) {
        CaptWarn("Unknown channel: " << id);
        return 1.0*unit::microsecond;
    }

    return row->GetASICPeakTime()*unit::ns;
}

double CP::TChannelCalib::GetAveragePulseShapeRise(CP::TChannelId id,
                                                   int order) {
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    if (!ev) {
        CaptError("No event is loaded so context cannot be set.");
        throw EChannelCalibUnknownType();
    }
    CP::TEventContext context = ev->GetContext();
    
    if (id.IsMCChannel()) return GetPulseShapeRise(id,order);

    CP::TResultSetHandle<CP::TTPC_Channel_Calib_Table> table(context);
    
    Int_t numChannels(table.GetNumRows());

    double riseShape = 0.0;
    double count = 0.0;
    for (int i = 0; i<numChannels; ++i) {
        const CP::TTPC_Channel_Calib_Table* row = table.GetRow(i);
        riseShape += row->GetASICRiseShape()*unit::ns;
        count += 1.0;
    }

    riseShape /= count;

    return riseShape;
}

double CP::TChannelCalib::GetPulseShapeRise(CP::TChannelId id, int order) {
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    if (!ev) {
        CaptError("No event is loaded so context cannot be set.");
        throw EChannelCalibUnknownType();
    }
    CP::TEventContext context = ev->GetContext();

    if (id.IsMCChannel()) {
        TMCChannelId mc(id);
        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptWarn("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        // Get the rising edge shape.
        CP::THandle<CP::TRealDatum> riseVect
            = ev->Get<CP::TRealDatum>("~/truth/elecSimple/shapeRise");
        double riseShape = (*riseVect)[index];
        return riseShape;
    }

    CP::TResultSetHandle<CP::TTPC_Channel_Calib_Table> table(context);
    const CP::TTPC_Channel_Calib_Table* row = table.GetRowByIndex(id.AsUInt());

    if (!row) {
        CaptWarn("Unknown channel: " << id);
        return 1.5;
    }

    return row->GetASICRiseShape();
}

double CP::TChannelCalib::GetAveragePulseShapeFall(CP::TChannelId id,
                                                   int order) {
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    if (!ev) {
        CaptError("No event is loaded so context cannot be set.");
        throw EChannelCalibUnknownType();
    }
    CP::TEventContext context = ev->GetContext();
    
    if (id.IsMCChannel()) return GetPulseShapeFall(id,order);

    CP::TResultSetHandle<CP::TTPC_Channel_Calib_Table> table(context);
    
    Int_t numChannels(table.GetNumRows());

    double fallShape = 0.0;
    double count = 0.0;
    for (int i = 0; i<numChannels; ++i) {
        const CP::TTPC_Channel_Calib_Table* row = table.GetRow(i);
        fallShape += row->GetASICFallShape()*unit::ns;
        count += 1.0;
    }

    fallShape /= count;

    return fallShape;
}

double CP::TChannelCalib::GetPulseShapeFall(CP::TChannelId id, int order) {
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    if (!ev) {
        CaptError("No event is loaded so context cannot be set.");
        throw EChannelCalibUnknownType();
    }
    CP::TEventContext context = ev->GetContext();

    if (id.IsMCChannel()) {
        TMCChannelId mc(id);
        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptWarn("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        // Get the falling edge shape.
        CP::THandle<CP::TRealDatum> fallVect
            = ev->Get<CP::TRealDatum>("~/truth/elecSimple/shapeFall");
        double fallShape = (*fallVect)[index];
        return fallShape;
    }

    CP::TResultSetHandle<CP::TTPC_Channel_Calib_Table> table(context);
    const CP::TTPC_Channel_Calib_Table* row = table.GetRowByIndex(id.AsUInt());

    if (!row) {
        CaptWarn("Unknown channel: " << id);
        return 1.7;
    }

    return row->GetASICFallShape();
}


double CP::TChannelCalib::GetPulseShape(CP::TChannelId id, double t) {
    if (t < 0.0) return 0.0;

    double peakingTime = GetPulseShapePeakTime(id);
    double riseShape = GetPulseShapeRise(id);
    double fallShape = GetPulseShapeFall(id);

    double arg = t/peakingTime;
    if (arg < 1.0) arg = std::pow(arg,riseShape);
    else arg = std::pow(arg,fallShape);
    
    double v = (arg<40)? arg*std::exp(-arg): 0.0;

    return 2.71828*v;
}

double CP::TChannelCalib::GetAveragePulseShape(CP::TChannelId id, double t) {
    if (t < 0.0) return 0.0;

    double peakingTime = GetAveragePulseShapePeakTime(id);
    double riseShape = GetAveragePulseShapeRise(id);
    double fallShape = GetAveragePulseShapeFall(id);

    double arg = t/peakingTime;
    if (arg < 1.0) arg = std::pow(arg,riseShape);
    else arg = std::pow(arg,fallShape);
    
    double v = (arg<40)? arg*std::exp(-arg): 0.0;

    return 2.71828*v;
}

double CP::TChannelCalib::GetTimeConstant(CP::TChannelId id, int order) {
    if (id.IsMCChannel()) {
        TMCChannelId mc(id);

        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptWarn("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
        
        // Get the digitization step.
        CP::THandle<CP::TRealDatum> stepVect
            = ev->Get<CP::TRealDatum>("~/truth/elecSimple/digitStep");
        double digitStep = (*stepVect)[index];

        // Get the trigger offset.
        CP::THandle<CP::TRealDatum> offsetVect
            = ev->Get<CP::TRealDatum>("~/truth/elecSimple/triggerOffset");
        double triggerOffset = (*offsetVect)[index];

        if (order == 0) {
            // The offset for the digitization time for each type of MC channel.
            // This was determined "empirically", and depends on the details of
            // the simulation.  It should be in the parameter file.  The exact
            // value depends on the details of the time clustering.
            double timeOffset = -triggerOffset;
            if (index == 1) timeOffset += -digitStep + 23*unit::ns;
            else if (index == 2) timeOffset += -digitStep + 52*unit::ns;
            return timeOffset;
        }
        else if (order == 1) {
            return digitStep;
        }

        return 0.0;
    }

    /// The time offset frore the TPC trigger is fixed to 3200 samples in
    /// software.  This isn't going to change, so just provide the fixed
    /// value.  The sample rate of 500 ns/sample is fixed in hardware against
    /// a very good clock, so it is also fixed.
    if (order == 0) return -1.600*unit::ms;
    if (order == 1) return 500.0*unit::ns;
    return 0.0;
}

double CP::TChannelCalib::GetDigitizerConstant(CP::TChannelId id, int order) {
    if (id.IsMCChannel()) {
        TMCChannelId mc(id);

        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptWarn("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
        
        if (order == 0) {
            // Get the pedestal
            CP::THandle<CP::TRealDatum> pedVect
                = ev->Get<CP::TRealDatum>("~/truth/elecSimple/pedestal");
            return (*pedVect)[index];
        }
        else if (order == 1) {
            // Get the digitizer slope
            CP::THandle<CP::TRealDatum> slopeVect
                = ev->Get<CP::TRealDatum>("~/truth/elecSimple/slope");
            return (*slopeVect)[index];
        }

        return 0.0;
    }

    // This is fixed for the data since we use an injected pulse with a known
    // charge to calibrate the ASIC and the digitizer as a combined system.
    // The precise value doesn't matter, so we are using the design spec.  The
    // actual digitizers vary by about 20%.
    if (order == 1) return 2.5/unit::mV;
    else if (order == 0) {
        CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
        if (!ev) {
            CaptError("No event is loaded so context cannot be set.");
            throw EChannelCalibUnknownType();
        }
        CP::TEventContext context = ev->GetContext();

        CP::TResultSetHandle<CP::TTPC_Channel_Calib_Table> table(context);
        const CP::TTPC_Channel_Calib_Table* row
            = table.GetRowByIndex(id.AsUInt());
        
        if (!row) {
            CaptWarn("Unknown channel: " << id);
            return 2048;
        }
        
        return row->GetDigitizerPedestal();
    }
    return 0.0;
}

double CP::TChannelCalib::GetElectronLifetime() {
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    
    // Get the electron lifetime.
    CP::THandle<CP::TRealDatum> stepVect
        = ev->Get<CP::TRealDatum>("~/truth/elecSimple/argon");
    if (!stepVect) return 3.14E+8*unit::second;
    return (*stepVect)[1];
}

double CP::TChannelCalib::GetElectronDriftVelocity() {
    CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
    
    // Get the electron lifetime.
    CP::THandle<CP::TRealDatum> stepVect
        = ev->Get<CP::TRealDatum>("~/truth/elecSimple/argon");
    if (!stepVect) return 1.6*unit::millimeter/unit::microsecond;
    return (*stepVect)[0];
}

double CP::TChannelCalib::GetCollectionEfficiency(CP::TChannelId id) {
    if (id.IsMCChannel()) {
        TMCChannelId mc(id);

        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) return 1.0;
        else {
            CaptWarn("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        if (index == 0) {
            // A X channel.
            double eff = CP::TRuntimeParameters::Get().GetParameterD(
                "clusterCalib.mc.wire.collection.x");
            return eff;
        }
        else if (index == 1) {
            // A V channel.
            double eff = CP::TRuntimeParameters::Get().GetParameterD(
                "clusterCalib.mc.wire.collection.v");
            return eff;
        }
        else if (index == 2) {
            // A U channel.
            double eff = CP::TRuntimeParameters::Get().GetParameterD(
                "clusterCalib.mc.wire.collection.u");
            return eff;
        }
    }

    return 1.0;
}

