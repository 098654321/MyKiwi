#include "./track_bits.hh"
#include <format>
#include <stdexcept>


namespace kiwi::algo {

// notice: the third param needs a track_index, not a group_index
TrackGroupCoord::TrackGroupCoord(const hardware::Coord& coord, hardware::TrackDirection dir, std::usize track_index) :
    _coord{coord}, _dir{dir}, _group_index{track_index / TrackGroupSize}
{}

TrackGroupCoord::TrackGroupCoord(const hardware::TrackCoord& coord) :
    _coord{hardware::Coord{coord.row, coord.col}}, _dir{coord.dir}, _group_index{coord.index / TrackGroupSize}
{}

auto TrackGroupCoord::to_string() const -> std::String {
    std::String direction {this->_dir == hardware::TrackDirection::Vertical ? "vertical" : "horizontal"};
    return std::format("({}, {}, {}, {})", this->_coord.row, this->_coord.col, direction, this->_group_index);
}

auto GlobalTrackGroups::track_group(const TrackGroupCoord& coord) -> BitsGroup<TrackGroupSize>& {
    return this->_track_groups.emplace(coord, BitsGroup<TrackGroupSize>{}).first->second;
}

auto GlobalTrackGroups::track_group(const hardware::TrackCoord& coord) -> BitsGroup<TrackGroupSize>& {
    return this->track_group(TrackGroupCoord::trackcoord_to_groupcoord(coord));
}

auto GlobalTrackGroups::record_track(const hardware::TrackCoord& coord, bool reuse_type) -> void {
    auto group_coord = TrackGroupCoord::trackcoord_to_groupcoord(coord);
    auto& group = this->track_group(coord);
    reuse_type ? group.add_reuse_number() : group.add_nonreuse_number();
}

auto GlobalTrackGroups::show() const -> void {
    for (auto& [coord, group]: this->_track_groups) {
        debug::info_fmt("{}:\n {}", coord.to_string(), group.to_string());
    }
    debug::info("\n");
}

// return <not_used, monopolized_by_reuse, has_nonreuse>
auto GlobalTrackGroups::info() const -> std::Tuple<std::usize, std::usize> {
    auto monopolized_by_reuse{0}, has_nonreuse{0};
    for (const auto& [coord, group]: this->_track_groups) {
        auto reuse = group.reuse_number();
        auto nonreuse = group.nonreuse_number();

        if (reuse > 0 && !nonreuse) {
            monopolized_by_reuse++;
        }
        else if (nonreuse > 0) {
            has_nonreuse++;
        }
    }
    return std::Tuple<std::usize, std::usize>{
        monopolized_by_reuse, has_nonreuse
    };
}

}

namespace std {

    std::size_t hash<kiwi::algo::TrackGroupCoord>::operator() (const kiwi::algo::TrackGroupCoord& coord) const noexcept {
        auto coord_hash = hash<kiwi::hardware::Coord>();
        auto dir_hash = hash<kiwi::hardware::TrackDirection>();
        auto usize_hash = hash<std::size_t>();

        return coord_hash(coord._coord) ^
               (dir_hash(coord._dir) << 1) ^
               (usize_hash(coord._group_index) << 2);
    }

}

