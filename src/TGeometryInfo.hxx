#ifndef TGeometryInfo_hxx_seen
#define TGeometryInfo_hxx_seen

#include <TEventContext.hxx>
#include <TChannelId.hxx>
#include <TGeometryId.hxx>

namespace CP {
    class TGeometryInfo;
};

class TGeoManager;

/// A singleton class to provide general information about the geometry.  This
/// requires that a valid geometry have been loaded.
class CP::TGeometryInfo {
public:
    /// Return a reference to the singleton.
    static CP::TGeometryInfo& Get(void);

    /// Get the best number wire in the requested plane.  The wires are
    /// counted from zero to "wireCount-1".
    int GetWire(int plane, double x, double y, double z = 0);

    /// Get the best X wire.
    int GetXWire(double x, double y, double z = 0);

    /// Get the best V wire.
    int GetVWire(double x, double y, double z = 0);
    
    /// Get the best U wire.
    int GetUWire(double x, double y, double z = 0);

    /// Get the total number of wires in a plane.
    int GetWireCount(int plane);

    /// Get the number of wires in the X plane.
    int GetXWireCount();
    
    /// Get the number of wires in the V plane.
    int GetVWireCount();

    /// Get the number of wires in the U plane.
    int GetUWireCount();

private: 
    /// An internal class used to hold cached information about the individual
    /// wires.
    struct WireCache {
        int fWire;
        // The position of the center of the wire.
        TVector3 fPosition;
        // A vector in the wire plane pointing perpendicular to the wire.
        TVector3 fPlane;
        // The length of the wire
        float fLength;
    };

    /// The instance pointer
    static CP::TGeometryInfo* fGeometryInfo;

    /// Make all of the constructors private.
    /// @{
    TGeometryInfo();
    TGeometryInfo(const TGeometryInfo&);
    TGeometryInfo& operator=(const TGeometryInfo&);
    /// @}

    /// The mostly recently seen geometry pointer.  This is used to decide if
    /// any internal caches need to be invalidated.
    TGeoManager* fGeometry;

    /// Fill the cache of wire geometries
    void FillWireCache();

    /// A vector of the X wires.
    std::vector<WireCache> fWires[3];

};
#endif
