#include "TChannelInfo.hxx"

#include <TCaptLog.hxx>

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

CP::TChannelId GetChannel(CP::TGeometryId id, int index) {
    return CP::TChannelId();
}

int GetChannelCount(CP::TGeometryId id) {
    return 1;
}

CP::TGeometryId GetGeometry(CP::TChannelId id, int index) {
    return CP::TGeometryId();
}

int GetGeometryCount(CP::TChannelId id) {
    return 1;
}
