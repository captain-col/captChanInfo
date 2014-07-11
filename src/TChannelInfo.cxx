#include "TChannelInfo.hxx"

#include <TCaptLog.hxx>
#include <CaptGeomId.hxx>
#include <TChannelId.hxx>
#include <TMCChannelId.hxx>
#include <TGeometryId.hxx>

// Initialize the singleton pointer.
CP::TChannelInfo* CP::TChannelInfo::fChannelInfo = NULL;

CP::TChannelInfo& CP::TChannelInfo::Get() {
    if (!fChannelInfo) {
        CaptVerbose("Create a new CP::TChannelInfo object");
        fChannelInfo = new CP::TChannelInfo();
    }
    return *fChannelInfo;
}

CP::TChannelInfo::TChannelInfo() {
    // Nothing to do at the moment...
}

void CP::TChannelInfo::SetContext(const CP::TEventContext& context) {
    // This needs to check if a new mapping needs to be loaded.
    fContext = context;
}

const CP::TEventContext& CP::TChannelInfo::GetContext() const {
    if (fContext.IsValid()) return fContext;
    CaptError("Event context must be set before using TChannelInfo.");
    return fContext;
}

CP::TChannelId CP::TChannelInfo::GetChannel(CP::TGeometryId id, int index) {
    // At the moment, index is always zero.
    if (index != 0) return CP::TChannelId();

    // Make sure that the identifier is valid.
    if (!id.IsValid()) {
        CaptError("Invalid geometry id cannot be translated to a channel");
        return CP::TChannelId();
    }

    // Make sure that we know what the current context is.  The channel to
    // geometry mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptError("Need valid event context to translate geometry to channel");
        return CP::TChannelId();
    }

    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    if (GetContext().IsMC()) {
        if (CP::GeomId::Captain::IsWire(id)) {
            return CP::TMCChannelId(
                0,
                CP::GeomId::Captain::GetWirePlane(id),
                CP::GeomId::Captain::GetWireNumber(id));
        }
        else if (CP::GeomId::Captain::IsPhotosensor(id)) {
            return CP::TMCChannelId(
                1, 0,
                CP::GeomId::Captain::GetPhotosensor(id));
        }
        return  CP::TChannelId();
    }
    
    // The context is valid, and not for the MC, so it should be for the
    // detector.  This shouldn't never happen, but it might.
    if (!GetContext().IsDetector()) {
        CaptError("Channel requested for invalid event context");
        return CP::TChannelId();
    }

    CaptError("Geometry to Channel lookup not implemented for the detector.");
    
    return CP::TChannelId();
}

int CP::TChannelInfo::GetChannelCount(CP::TGeometryId id) {
    if (!id.IsValid()) return 0;
    return 1;
}

CP::TGeometryId CP::TChannelInfo::GetGeometry(CP::TChannelId i, int index) {
    // At the moment, index is always zero.
    if (index != 0) return CP::TGeometryId();

    // Make sure that we have an event context since the channel to geometry
    // mapping changes with time.
    if (!GetContext().IsValid()) {
        CaptWarn("Need valid event context to translate channel to geometry");
        return CP::TGeometryId();
    }

    // Make sure this is a valid channel and flag an error if not.
    if (!i.IsValid()) {
        CaptError("Invalid channel cannot be translated to geometry");
        return CP::TGeometryId();
    }

    // The current context is for the MC, so the channel can be generated
    // algorithmically.
    if (i.IsMCChannel()) {
        CP::TMCChannelId id(i);
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
    if (!GetContext().IsDetector()) {
        CaptError("Channel requested for invalid event context");
        return CP::TGeometryId();
    }
    
    CaptError("Channel to Geometry lookup not implemented for the detector.");
    
    return CP::TGeometryId();
}

int CP::TChannelInfo::GetGeometryCount(CP::TChannelId id) {
    if (!id.IsValid()) return 0;
    return 1;
}
