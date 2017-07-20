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

#include <TResultSetHandle.hxx>
#include <DatabaseUtils.hxx>

#define SKIP_DATA_CALIBRATION

namespace {
    // This is slightly evil, but keep a "local" cache of the most recent
    // calibration coefficients.

    // A cache for the bad channel table.
    CP::TEventContext gTPCBadChannelContext;
    typedef std::map<CP::TChannelId,int> TPCBadChannelMap;
    TPCBadChannelMap gTPCBadChannels;
    void UpdateTPCBadChannels() {
        CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
        if (!ev) {
            gTPCBadChannels.clear();
            return;
        }
        CP::TEventContext context = ev->GetContext();
        if (context == gTPCBadChannelContext) return;
        gTPCBadChannelContext = context;
        gTPCBadChannels.clear();
        
        CaptLog("Bad channel table update: " << gTPCBadChannelContext);
        
        // Get the bad channel table.
        CP::TResultSetHandle<CP::TTPC_Bad_Channel_Table> chanTable(context);
        Int_t numChannels(chanTable.GetNumRows());

        for (int i = 0; i<numChannels; ++i) {
            const CP::TTPC_Bad_Channel_Table* chanRow = chanTable.GetRow(i);
            if (!chanRow) continue;
            CP::TChannelId chanId = chanRow->GetChannelId();
            gTPCBadChannels[chanId] = chanRow->GetChannelStatus();
        }
    }
}

CP::TChannelCalib::TChannelCalib() { }

CP::TChannelCalib::~TChannelCalib() { } 

bool CP::TChannelCalib::IsGoodChannel(CP::TChannelId id) {
    int status = GetChannelStatus(id);
    if (status > 0) return false;
    return true;
}

bool CP::TChannelCalib::IsBipolarSignal(CP::TChannelId id) {
    if (id.IsMCChannel()) {
        TMCChannelId mc(id);

        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptError("Unknown channel: " << id);
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

    CaptError("Unknown channel: " << id);
    throw EChannelCalibUnknownType();
    return false;
}

int CP::TChannelCalib::GetChannelStatus(CP::TChannelId id) {
    if (id.IsMCChannel()) return 0;
    UpdateTPCBadChannels();
    TPCBadChannelMap::iterator val = gTPCBadChannels.find(id);
    if (val == gTPCBadChannels.end()) return -1;
    return val->second;
}

double CP::TChannelCalib::GetGainConstant(CP::TChannelId id, int order) {
    if (id.IsMCChannel()) {
        TMCChannelId mc(id);

        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptError("Unknown channel: " << id);
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
            // Get the gain
            CP::THandle<CP::TRealDatum> gainVect
                = ev->Get<CP::TRealDatum>("~/truth/elecSimple/gain");
            return (*gainVect)[index];
        }

        return 0.0;
    }

#ifdef SKIP_DATA_CALIBRATION
    if (order == 1) return (14.0*unit::mV/unit::fC);
    return 2048.0;
#endif

    CaptError("Unknown channel: " << id);
    throw EChannelCalibUnknownType();
    return 0.0;
}

double CP::TChannelCalib::GetPulseShape(CP::TChannelId id, double t) {
    if (id.IsMCChannel()) {
        if (t < 0.0) return 0.0;

        TMCChannelId mc(id);
        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptError("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
        
        // Get the rise time.
        CP::THandle<CP::TRealDatum> shapeVect
            = ev->Get<CP::TRealDatum>("~/truth/elecSimple/shape");
        double peakingTime = (*shapeVect)[index];

        // Get the rising edge shape.
        CP::THandle<CP::TRealDatum> riseVect
            = ev->Get<CP::TRealDatum>("~/truth/elecSimple/shapeRise");
        double riseShape = (*riseVect)[index];
        
        // Get the rising edge shape.
        CP::THandle<CP::TRealDatum> fallVect
            = ev->Get<CP::TRealDatum>("~/truth/elecSimple/shapeFall");
        double fallShape = (*fallVect)[index];
        
        double arg = t/peakingTime;
        if (arg < 1.0) arg = std::pow(arg,riseShape);
        else arg = std::pow(arg,fallShape);
        
        double v = (arg<40)? arg*std::exp(-arg): 0.0;
        return v;
    }

#ifdef SKIP_DATA_CALIBRATION
    if (t < 0.0) return 0.0;

    double peakingTime = 1.0*unit::microsecond;
    double riseShape = 2.0;
    double fallShape = 2.0;

    double arg = t/peakingTime;
    if (arg < 1.0) arg = std::pow(arg,riseShape);
    else arg = std::pow(arg,fallShape);
    
    double v = (arg<40)? arg*std::exp(-arg): 0.0;

    return v;
#endif
    
    CaptError("Unknown channel: " << id);
    throw EChannelCalibUnknownType();
    return 0.0;
}

double CP::TChannelCalib::GetTimeConstant(CP::TChannelId id, int order) {
    if (id.IsMCChannel()) {
        TMCChannelId mc(id);

        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptError("Unknown channel: " << id);
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

#ifdef SKIP_DATA_CALIBRATION
    if (order == 0) return -1.600*unit::ms;
    if (order == 1) return 500.0*unit::ns;
    return 0.0;
#endif

    CaptError("Unknown channel: " << id);
    throw EChannelCalibUnknownType();
    return 0.0;
}

double CP::TChannelCalib::GetDigitizerConstant(CP::TChannelId id, int order) {
    if (id.IsMCChannel()) {
        TMCChannelId mc(id);

        int index = -1;
        if (mc.GetType() == 0) index = mc.GetSequence();
        else if (mc.GetType() == 1) index = 3;
        else {
            CaptError("Unknown channel: " << id);
            throw CP::EChannelCalibUnknownType();
        }
            
        CP::TEvent* ev = CP::TEventFolder::GetCurrentEvent();
        
        if (order == 0) {
            return 0.0;
        }
        else if (order == 1) {
            // Get the digitizer slope
            CP::THandle<CP::TRealDatum> slopeVect
                = ev->Get<CP::TRealDatum>("~/truth/elecSimple/slope");
            return (*slopeVect)[index];
        }

        return 0.0;
    }

#ifdef SKIP_DATA_CALIBRATION
    if (order == 1) return 2.5/unit::mV;
    return 0.0;
#endif

    CaptError("Unknown channel: " << id);
    throw EChannelCalibUnknownType();
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
            CaptError("Unknown channel: " << id);
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

#ifdef SKIP_DATA_CALIBRATION
    return 1.0;
#endif


    CaptError("Unknown channel: " << id);
    throw EChannelCalibUnknownType();
    return 1.0;
}

