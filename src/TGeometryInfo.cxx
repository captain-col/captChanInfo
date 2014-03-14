#include "TGeometryInfo.hxx"

#include <TCaptLog.hxx>
#include <CaptGeomId.hxx>
#include <TGeometryId.hxx>
#include <TManager.hxx>
#include <TGeomIdManager.hxx>
#include <HEPUnits.hxx>

#include <TGeoManager.h>
#include <TGeoVolume.h>
#include <TGeoShape.h>
#include <TGeoBBox.h>

// Initialize the singleton pointer.
CP::TGeometryInfo* CP::TGeometryInfo::fGeometryInfo = NULL;

CP::TGeometryInfo& CP::TGeometryInfo::Get() {
    if (!fGeometryInfo) {
        CaptVerbose("Create a new CP::TGeometryInfo object");
        fGeometryInfo = new CP::TGeometryInfo();
    }
    return *fGeometryInfo;
}

CP::TGeometryInfo::TGeometryInfo() {
    fGeometry = NULL;
}

int CP::TGeometryInfo::GetXWire(double x, double y, double z) {
    return GetWire(CP::GeomId::Captain::kXPlane,x,y,z);
}

int CP::TGeometryInfo::GetVWire(double x, double y, double z) {
    return GetWire(CP::GeomId::Captain::kVPlane,x,y,z);
}

int CP::TGeometryInfo::GetUWire(double x, double y, double z) {
    return GetWire(CP::GeomId::Captain::kUPlane,x,y,z);
}

int CP::TGeometryInfo::GetXWireCount() {
    return GetWireCount(CP::GeomId::Captain::kXPlane);
}

int CP::TGeometryInfo::GetVWireCount() {
    return GetWireCount(CP::GeomId::Captain::kVPlane);
}

int CP::TGeometryInfo::GetUWireCount() {
    return GetWireCount(CP::GeomId::Captain::kUPlane);
}

int CP::TGeometryInfo::GetWire(int plane, double x, double y, double z) {
    if (plane<0) return -1;
    if (plane>2) return -1;
    FillWireCache();
    
    TVector3 pos(x,y,z);
    
    // This code could be faster, but I don't think it's a bottleneck.
    int wireCount = fWires[plane].size();
    double minDist= 100*unit::km;
    int bestWire = -1;
    for (int w = 0; w<wireCount; ++w) {
        double d = (pos-fWires[plane][w].fPosition)*fWires[plane][w].fPlane;
        if (d < minDist) {
            minDist = d;
            bestWire = w;
        }
    }

    return bestWire;
}

int CP::TGeometryInfo::GetWireCount(int plane) {
    if (plane<0) return 0;
    if (plane>2) return 0;
    FillWireCache();
    return fWires[plane].size();
}

void CP::TGeometryInfo::FillWireCache() {

    // Get the current geometry.  The fGeometry field holds the last geometry
    // chached.  If it disagrees with gGeoManager, then the cache needs
    // refilling.
    CP::TManager::Get().Geometry();
    if (fGeometry == gGeoManager) return;
    fGeometry = gGeoManager;

    gGeoManager->PushPath();

    for (int i=0; i<3; ++i) fWires[i].clear();
    
    // For each wire in the detector fill the cache.  The loop is done
    // this way so that we don't need to know how many planes and wires are
    // in the detector.  This is being too cautious, but costs very little.
    for (int plane = 0; plane < 3; ++plane) {
        // Check to see if this plane exists, quit looking if doesn't.
        if (!CP::TManager::Get().GeomId().CdId(
                CP::GeomId::Captain::Plane(plane))) break;
        for (int wire = 0; wire < 10000; ++wire) {
            // Check to see if the wire exists.  Quit looking if it doesn't.
            if (!CP::TManager::Get().GeomId().CdId(
                    CP::GeomId::Captain::Wire(plane,wire))) break;
            // Arrays for the transforms.
            double local[3];
            double master[3];
            WireCache cache;
            cache.fWire = wire;
            // Find the center of the wire.
            local[0] = local[1] = local[2] = 0.0;
            gGeoManager->LocalToMaster(local,master);
            cache.fPosition = TVector3(master);
            // Find the perpendicular to the wire.
            local[0] = local[1] = local[2] = 0.0;
            local[0] = 1.0;
            gGeoManager->LocalToMasterVect(local,master);
            cache.fPlane = TVector3(master);
            // Find the length of the wire.
            TGeoBBox* box = dynamic_cast<TGeoBBox*>(
                gGeoManager->GetCurrentVolume()->GetShape());
            cache.fLength = box->GetDY();
            fWires[plane].push_back(cache);
        }
    }

    gGeoManager->PopPath();

}
