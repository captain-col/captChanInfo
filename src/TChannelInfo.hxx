#ifndef TChannelInfo_hxx_seen
#define TChannelInfo_hxx_seen

#include <TEventContext.hxx>
#include <TChannelId.hxx>
#include <TGeometryId.hxx>

namespace CP {
    class TChannelInfo;
};

/// A singleton class to translate between channel identifiers,
/// CP::TChannelId(), and geometry identifiers, CP::TGeometryId().  This
/// provides a two way map.  To be used, the event context needs to have been
/// set using the SetContext method.  Generally, SetContext() should be called
/// when a new event is handled.  
class CP::TChannelInfo {
public:
    /// Return a reference to the singleton.
    static CP::TChannelInfo& Get(void);

    /// Set the event context to be used when mapping identifiers.  This
    /// should be set before accessing the identifiers.  This may trigger a
    /// database access if the context is changing.  Explicitly setting the
    /// context is required since this allows lookups even when an event is
    /// not available.
    void SetContext(const CP::TEventContext& context);

    /// Get the event context being used for mapping identifiers.  If the
    /// context has not been set explicitly, then it will try the value for
    /// the current event.
    const CP::TEventContext& GetContext() const;

    /// Map a geometry identifier into a channel identifier.  This takes an
    /// optional index for when there is more than one electronics channel per
    /// geometry object.  An example of this might be a multi-hit tdc where
    /// each hit is recorded as a different channel.  The FNAL TripT chip is
    /// an example where each hit gets a different channel.
    CP::TChannelId GetChannel(CP::TGeometryId id, int index=0);

    /// Get the number of electronics channels that map to a particular
    /// geometry identifier.
    int GetChannelCount(CP::TGeometryId id);

    /// Map a geometry identifier into a channel identifier.  This takes an
    /// optional index for when there is more than one electronics channel per
    /// geometry object.  An example of this might be a multi-hit tdc where
    /// each hit is recorded as a different channel.  The FNAL TripT chip is
    /// an example where each hit gets a different channel.
    CP::TGeometryId GetGeometry(CP::TChannelId id, int index=0);

    /// Get the number of electronics channels that map to a particular
    /// geometry identifier.
    int GetGeometryCount(CP::TChannelId id);

private: 
    /// The instance pointer
    static CP::TChannelInfo* fChannelInfo;

    /// Make all of the constructors private.
    /// @{
    TChannelInfo();
    TChannelInfo(const TChannelInfo&);
    TChannelInfo& operator=(const TChannelInfo&);
    /// @}

    /// The event context to be used to map identifiers
    CP::TEventContext fContext;

};
#endif
