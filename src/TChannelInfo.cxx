#include "TChannelInfo.hxx"

#include <TCaptLog.hxx>
#include <CaptGeomId.hxx>
#include <TChannelId.hxx>
#include <TMCChannelId.hxx>
#include <TTPCChannelId.hxx>
#include <TGeometryId.hxx>
#include <TCaptLog.hxx>

#include <TTPC_Wire_Channel_Table.hxx>
#include <TTPC_Wire_Geometry_Table.hxx>

#include <TResultSetHandle.hxx>
#include <DatabaseUtils.hxx>

#include <TSystem.h>

#include <fstream>
#include <string>
#include <sstream>

// Initialize the singleton pointer.
CP::TChannelInfo* CP::TChannelInfo::fChannelInfo = NULL;

CP::TChannelInfo& CP::TChannelInfo::Get() {
    if (!fChannelInfo) {
        fChannelInfo = new CP::TChannelInfo();
    }
    return *fChannelInfo;
}

CP::TChannelInfo::TChannelInfo() {
    const char* envVal = gSystem->Getenv("CAPTCHANNELMAP");
    if (!envVal) return;

    std::string mapName(envVal);

    if (mapName.empty()) return;

    // Attach the file to a stream.
    std::ifstream mapFile(mapName.c_str());
    std::string line;
    while (std::getline(mapFile,line)) {
        std::size_t comment = line.find("#");
        std::string tmp = line;
        if (comment != std::string::npos) tmp.erase(comment);
        // The minimum valid date, time and hash is 22 characters.  
        if (tmp.size()<20) continue;
        std::istringstream parser(tmp);
        int crate;
        int fem;
        int channel;
        int detector;
        int plane;
        int wire;
        
        parser >> crate;
        if (parser.fail()) {
            CaptError("Could not parse crate number: "<< line);
            continue;
        }

        parser >> fem;
        if (parser.fail()) {
            CaptError("Could not parse fem number: "<< line);
            continue;
        }

        parser >> channel;
        if (parser.fail()) {
            CaptError("Could not parse channel number: "<< line);
            continue;
        }

        parser >> detector;
        if (parser.fail()) {
            CaptError("Could not parse detector number: "<< line);
            continue;
        }

        parser >> plane;
        if (parser.fail()) {
            CaptError("Could not parse plane number: "<< line);
            continue;
        }

        parser >> wire;
        if (parser.fail()) {
            CaptError("Could not parse wire number: "<< line);
            continue;
        }

        CP::TChannelId cid;
        CP::TGeometryId gid;
        if (detector == CP::TChannelId::kTPC) {
            cid = CP::TTPCChannelId(crate,fem,channel);
            gid = CP::GeomId::Captain::Wire(plane,wire);
        }
        else {
            CaptError("Unknown detector channel: " << line);
            continue;
        }

        if (fChannelMap.find(cid) != fChannelMap.end()) {
            CaptError("Channel already exists: " << line);
            CaptError("   Duplicate " << gid);
        }

        if (fGeometryMap.find(gid) != fGeometryMap.end()) {
            CaptError("Channel already exists: " << line);
            CaptError("   Duplicate " << cid);
        }

        fChannelMap[cid] = gid;
        fGeometryMap[gid] = cid;

    }
}

void CP::TChannelInfo::SetContext(const CP::TEventContext& context) {

    if (context == fContext) {
        CaptNamedInfo("TChannelInfo","context: " << context << " (no change)");
        return;
    }

    // This needs to check if a new mapping needs to be loaded.
    fContext = context;
    CaptNamedInfo("TChannelInfo","context: " << context << " (change)");

    if (!fContext.IsValid()) {
        CaptError("New event context is not valid: " << context);
        return;
    }
    
    if (fContext.IsMC()) {
        return;
    }
    
    // Get the channel table.
    CP::TResultSetHandle<CP::TTPC_Wire_Channel_Table> chanTable(context);
    Int_t numChannels(chanTable.GetNumRows());

    // Get the geometry table.
    CP::TResultSetHandle<CP::TTPC_Wire_Geometry_Table> geomTable(context);
    Int_t numGeometries(geomTable.GetNumRows());

    if (numChannels == 0 || numGeometries == 0) {
        if (numChannels == 0) {
            CaptError("Missing channel table for " << context);
        }
        if (numGeometries == 0) {
            CaptError("Missing geometry table for " << context);
        }
        return;
    }

    for (int i = 0; i<numChannels; ++i) {
        const CP::TTPC_Wire_Channel_Table* chanRow = chanTable.GetRow(i);
        if (!chanRow) {
            CaptError("Missing channel row " << i);
            continue;
        }

        CP::TChannelId chanId = chanRow->GetChannelId();
        int wire = chanRow->GetWire();
        int mb = chanRow->GetMotherBoard();
        int asic = chanRow->GetASIC();
        int asicChan = chanRow->GetASICChannel();
        fChannelToASICMap[chanId] = mb*1000*1000 + asic*1000 + asicChan;

        if (wire <= 0) continue;
        fChannelToWireMap[chanId] = wire;
        fWireToChannelMap[wire] = chanId;

        const CP::TTPC_Wire_Geometry_Table* geomRow 
            = geomTable.GetRowByIndex(wire);
        if (!geomRow) {
            CaptError("Missing geometry row " << chanId
                      << " --> " << wire );
            continue;
        }
        CP::TGeometryId geomId =  geomRow->GetGeometryId();
        fChannelMap[chanId] = geomId;
        fGeometryMap[geomId] = chanId;
    }

    for (int i = 0; i<numGeometries; ++i) {
        const CP::TTPC_Wire_Geometry_Table* geomRow 
            = geomTable.GetRow(i);
        if (!geomRow) {
            CaptError("Missing geometry row " << i);
            continue;
        }
        CP::TGeometryId geomId =  geomRow->GetGeometryId();
        int wire = geomRow->GetWire();
        if (wire < 0) continue;
        fGeometryToWireMap[geomId] = wire;
        fWireToGeometryMap[wire] = geomId;
    }

}

const CP::TEventContext& CP::TChannelInfo::GetContext() const {
    if (fContext.IsValid()) return fContext;
    CaptError("Event context must be set before using TChannelInfo.");
    return fContext;
}

CP::TChannelId CP::TChannelInfo::GetChannel(CP::TGeometryId gid, int index) {
    // At the moment, index is always zero.
    if (index != 0) return CP::TChannelId();

    // Make sure that the identifier is valid.
    if (!gid.IsValid()) {
        CaptError("Invalid geometry id cannot be translated to a channel");
        return CP::TChannelId();
    }

    // Make sure that we know what the current context is.  The channel to
    // geometry mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptError("Need valid event context to translate geometry to channel: "
                  << GetContext());
        return CP::TChannelId();
    }

    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    if (GetContext().IsMC()) {
        if (CP::GeomId::Captain::IsWire(gid)) {
            return CP::TMCChannelId(
                0,
                CP::GeomId::Captain::GetWirePlane(gid),
                CP::GeomId::Captain::GetWireNumber(gid));
        }
        else if (CP::GeomId::Captain::IsPhotosensor(gid)) {
            return CP::TMCChannelId(
                1, 0,
                CP::GeomId::Captain::GetPhotosensor(gid));
        }
        return  CP::TChannelId();
    }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
#ifdef CHECK_DETECTOR_CONTEXT
    if (!GetContext().IsDetector()) {
        CaptError("Channel requested for invalid event context");
        return CP::TChannelId();
    }
#endif

    std::map<CP::TGeometryId,CP::TChannelId>::iterator geometryEntry
        = fGeometryMap.find(gid);

    if (geometryEntry == fGeometryMap.end()) {
        CaptWarn("Channel for object not found: " << gid);
        return CP::TChannelId();
    }
        
    return geometryEntry->second;
}

CP::TChannelId CP::TChannelInfo::GetChannel(int wirenumber, int index) {
    // At the moment, index is always zero.
    if (index != 0) return CP::TChannelId();
    
    // Make sure that the identifier is valid.
    if (wirenumber == -1) {
        CaptError("Invalid channel id can not be translated to a wire number");
        return CP::TChannelId();
    }
    
    // Make sure that we know what the current context is.  The channel to
    // geometry mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptError("Need valid event context to translate channel id to wire number: "
                  << GetContext());
        return CP::TChannelId();
    }
    
    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    // need to add what to do for MC (this is unfinished, jieun, july25, 2015)
    //  if (GetContext().IsMC()) {
    //  }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
#ifdef CHECK_DETECTOR_CONTEXT
    if (!GetContext().IsDetector()) {
        CaptError("Channel requested for invalid event context");
        return CP::TChannelId();
    }
#endif

    std::map<int,CP::TChannelId>::iterator channelEntry
        = fWireToChannelMap.find(wirenumber);
    if (channelEntry == fWireToChannelMap.end()) {
        CaptWarn("Channel for object not found: " << wirenumber);
        return CP::TChannelId();
    }
        
    return channelEntry->second;
}

int CP::TChannelInfo::GetChannelCount(CP::TGeometryId id) {
    if (!id.IsValid()) return 0;
    return 1;
}

CP::TGeometryId CP::TChannelInfo::GetGeometry(CP::TChannelId cid) {
    // Make sure that we have an event context since the channel to geometry
    // mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptWarn("Need valid event context to translate channel to geometry");
        return CP::TGeometryId();
    }

    // Make sure this is a valid channel and flag an error if not.
    if (!cid.IsValid()) {
        CaptError("Invalid channel cannot be translated to geometry");
        return CP::TGeometryId();
    }

    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    if (cid.IsMCChannel()) {
        CP::TMCChannelId id(cid);
        if (id.GetType() == 0) {
            // This is a wire channel.
            return CP::GeomId::Captain::Wire(id.GetSequence(),id.GetNumber());
        }
        else if (id.GetType() == 1) {
            // This is a light sensor.
            return CP::GeomId::Captain::Photosensor(id.GetNumber());
        }
        return  CP::TGeometryId();
    }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
#ifdef CHECK_DETECTOR_CONTEXT
    if (!GetContext().IsDetector()) {
        CaptWarn("Channel requested for invalid event context");
        return CP::TGeometryId();
    }
#endif

    std::map<CP::TChannelId, CP::TGeometryId>::iterator channelEntry
        = fChannelMap.find(cid);

    if (channelEntry == fChannelMap.end()) {
        CaptWarn("Geometry for channel is not found: " << cid);
        return CP::TGeometryId();
    }
        
    return channelEntry->second;
}

CP::TGeometryId CP::TChannelInfo::GetGeometry(int wirenumber) {
    // Make sure that the identifier is valid.
    if (wirenumber == -1) {
        CaptError("Invalid wire number cannot be translated to a geometry");
        return CP::TGeometryId();
    }
    
    // Make sure that we know what the current context is.  The channel to
    // geometry mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptError("Invalid event context cannot be translated to geometry id: "
                  << GetContext());
        return CP::TGeometryId();
    }
    
    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    // need to add what to do for MC (this is unfinished, jieun, july25, 2015)
    //  if (GetContext().IsMC()) {
    //  }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
#ifdef CHECK_DETECTOR_CONTEXT
    if (!GetContext().IsDetector()) {
        CaptError("Channel requested for invalid event context");
        return CP::TChannelId();
    }
#endif

    std::map<int,CP::TGeometryId>::iterator geometryEntry
        = fWireToGeometryMap.find(wirenumber);
    if (geometryEntry == fWireToGeometryMap.end()) {
        CaptWarn("Geometry for object not found: " << wirenumber);
        return CP::TGeometryId();
    }
        
    return geometryEntry->second;
}

int CP::TChannelInfo::GetGeometryCount(CP::TChannelId id) {
    if (!id.IsValid()) return 0;
    return 1;
}

int CP::TChannelInfo::GetWireNumber(CP::TChannelId cid) {
    // Make sure that the identifier is valid.
    if (!cid.IsValid()) {
        CaptError("Invalid channel id can not be translated to a wire number");
        return -1;
    }
    
    // Make sure that we know what the current context is.  The channel to
    // geometry mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptError("Need valid event context to translate channel id to wire number: "
                  << GetContext());
        return -1;
    }
    
    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    // need to add what to do for MC (this is unfinished, jieun, july25, 2015)
    //  if (GetContext().IsMC()) {
    //  }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
#ifdef CHECK_DETECTOR_CONTEXT
    if (!GetContext().IsDetector()) {
        CaptError("Channel requested for invalid event context");
        return -1;
    }
#endif

    std::map<CP::TChannelId, int>::iterator wireEntry
        = fChannelToWireMap.find(cid);
    if (wireEntry == fChannelToWireMap.end()) {
        CaptWarn("Channel for object not found: " << cid);
        return -1;
    }
        
    return wireEntry->second;
}

int CP::TChannelInfo::GetWireNumber(CP::TGeometryId gid) {
    // Make sure that the identifier is valid.
    if (!gid.IsValid()) {
        CaptError("Invalid geometry id can not be translated to a wire number");
        return -1;
    }
    
    // Make sure that we know what the current context is.  The channel to
    // geometry mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptError("Need valid event context to translate geometry id to wire number: "
                  << GetContext());
        return -1;
    }
    
    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    // need to add what to do for MC (this is unfinished, jieun, july25, 2015)
    //  if (GetContext().IsMC()) {
    //  }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
#ifdef CHECK_DETECTOR_CONTEXT
    if (!GetContext().IsDetector()) {
        CaptError("Channel requested for invalid event context");
        return -1;
    }
#endif

    std::map<CP::TGeometryId, int>::iterator wireEntry
        = fGeometryToWireMap.find(gid);
    if (wireEntry == fGeometryToWireMap.end()) {
        CaptWarn("Geometry for object not found: " << gid);
        return -1;
    }
        
    return wireEntry->second;
}


int CP::TChannelInfo::GetMotherboard(CP::TChannelId cid) {
    // Make sure that we have an event context since the channel to geometry
    // mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptWarn("Need valid event context to translate channel to geometry");
        return -1;
    }

    // Make sure this is a valid channel and flag an error if not.
    if (!cid.IsValid()) {
        CaptError("Invalid channel cannot be translated to geometry");
        return -1;
    }

    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    if (cid.IsMCChannel()) {
        return -1;
    }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
    if (!GetContext().IsDetector()) {
        CaptWarn("Channel requested for invalid event context");
        return -1;
    }

    std::map<CP::TChannelId, int>::iterator channelEntry
        = fChannelToASICMap.find(cid);

    if (channelEntry == fChannelToASICMap.end()) {
        return -1;
    }
        
    return channelEntry->second/1000000;
}

int CP::TChannelInfo::GetASIC(CP::TChannelId cid) {
    // Make sure that we have an event context since the channel to geometry
    // mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptWarn("Need valid event context to translate channel to geometry");
        return -1;
    }

    // Make sure this is a valid channel and flag an error if not.
    if (!cid.IsValid()) {
        CaptError("Invalid channel cannot be translated to geometry");
        return -1;
    }

    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    if (cid.IsMCChannel()) {
        return -1;
    }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
    if (!GetContext().IsDetector()) {
        CaptWarn("Channel requested for invalid event context");
        return -1;
    }

    std::map<CP::TChannelId, int>::iterator channelEntry
        = fChannelToASICMap.find(cid);

    if (channelEntry == fChannelToASICMap.end()) {
        return -1;
    }
        
    return (channelEntry->second/1000) % 1000;
}

int CP::TChannelInfo::GetASICChannel(CP::TChannelId cid) {
    // Make sure that we have an event context since the channel to geometry
    // mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptWarn("Need valid event context to translate channel to geometry");
        return -1;
    }

    // Make sure this is a valid channel and flag an error if not.
    if (!cid.IsValid()) {
        CaptError("Invalid channel cannot be translated to geometry");
        return -1;
    }

    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    if (cid.IsMCChannel()) {
        return -1;
    }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
    if (!GetContext().IsDetector()) {
        CaptWarn("Channel requested for invalid event context");
        return -1;
    }

    std::map<CP::TChannelId, int>::iterator channelEntry
        = fChannelToASICMap.find(cid);

    if (channelEntry == fChannelToASICMap.end()) {
        return -1;
    }
        
    return channelEntry->second % 1000;
}

