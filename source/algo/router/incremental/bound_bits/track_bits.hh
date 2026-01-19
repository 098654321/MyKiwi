#pragma once

#include <global/std/collection.hh>
#include <global/std/integer.hh>
#include <hardware/track/track.hh>
#include "./bits_group.hh"
#include <algorithm>


namespace kiwi::algo {

inline constexpr std::usize TrackGroupSize = 32;

struct TrackGroupCoord {

    TrackGroupCoord(const hardware::Coord& coord, hardware::TrackDirection dir, std::usize track_index);
    TrackGroupCoord(const hardware::TrackCoord& coord);
    ~TrackGroupCoord() = default;

    static auto trackcoord_to_groupcoord(const hardware::TrackCoord& coord) -> TrackGroupCoord {
        return TrackGroupCoord{coord};
    }

    auto operator == (const TrackGroupCoord& other) const -> bool {
        return this->_coord == other._coord && this->_dir == other._dir && this->_group_index == other._group_index;
    }

    auto to_string() const -> std::String;

    hardware::Coord _coord;
    hardware::TrackDirection _dir;
    std::usize _group_index;
};

}

namespace std {

    template <>
    struct std::hash<kiwi::algo::TrackGroupCoord> {
        std::size_t operator() (const kiwi::algo::TrackGroupCoord& coord) const noexcept;
    };
    
}

namespace kiwi::algo {

class GlobalTrackGroups {

public:
    GlobalTrackGroups() : _track_groups{} {}
    ~GlobalTrackGroups() = default;

public:
    auto track_group(const TrackGroupCoord& coord) -> BitsGroup<TrackGroupSize>&;
    auto track_group(const hardware::TrackCoord& coord) -> BitsGroup<TrackGroupSize>&;

    auto record_track(const hardware::TrackCoord& coord, bool reuse_type) -> void;

    auto show() const -> void;
    auto info() const -> std::Tuple<std::usize, std::usize>;

private:
    std::HashMap<TrackGroupCoord, BitsGroup<TrackGroupSize>> _track_groups;
};

}




