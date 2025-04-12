#include "./hardware_recorder.hh"
#include <debug/debug.hh>
#include <ranges>


namespace kiwi::algo{

auto HardwareRecorder::get_track_recorder(hardware::Track* track, bool reuse_type) -> TypeRecorder& {
    auto iter = this->_track_recorders.emplace(track, TypeRecorder{reuse_type});
    return iter.first->second;
}

auto HardwareRecorder::get_track_recorder(hardware::Track* track, bool reuse_type) const -> const TypeRecorder& {
    return const_cast<HardwareRecorder*>(this)->get_track_recorder(track, reuse_type);
}

auto HardwareRecorder::get_tob_recorder(hardware::TOBCoord coord) -> TOBRecorder& {
    auto iter = this->_tob_recorders.emplace(coord, TOBRecorder{});
    return iter.first->second;
}

auto HardwareRecorder::get_cob_recorder(hardware::COBCoord coord) -> COBRecorder& {
    auto iter = this->_cob_recorders.emplace(coord, COBRecorder{});
    return iter.first->second;
}

auto HardwareRecorder::get_cob_type_recorder(
    hardware::COBCoord coord, std::usize unit_i, std::usize reg_i
) -> TypeRecorder& {
    auto unit_recorder = this->get_cob_recorder(coord).unit_recorder(unit_i);
    return unit_recorder.recorder(reg_i);
}

auto HardwareRecorder::bump_to_track_cost(hardware::TOBCoord coord, std::usize bump_index, hardware::Track* track, bool reuse_type) const -> float {
    auto track_index = track->coord().index;
    return this->_tob_recorders.at(coord).tob_cost(bump_index, track_index, reuse_type) + this->track_cost(track,reuse_type);
}

auto HardwareRecorder::track_cost(hardware::Track* track, bool reuse_type) const -> float {
    auto& type_recorder = this->get_track_recorder(track, reuse_type);

    auto track_coord = track->coord();
    auto group_index = track_coord.index / TRACKGROUPSIZE;

    auto reuse_num = 0, nonre_num = 0;
    for (auto i: std::views::iota((int)(TRACKGROUPSIZE*group_index), (int)(TRACKGROUPSIZE*group_index+TRACKGROUPSIZE-1))) {
        auto coord = hardware::TrackCoord(track_coord.row, track_coord.col, track_coord.dir, i);
        auto t = this->_interposer->get_track(coord);
        if (t.has_value() && this->_track_recorders.contains(t.value())) {
            auto track_recorder = this->_track_recorders.at(t.value());
            if (track_recorder.current_type().has_value()) {
                auto track_type = *track_recorder.current_type();
                track_type ? reuse_num++ : nonre_num++;
            }
        }
    }

    return type_recorder.cost(reuse_num, nonre_num);
}

auto HardwareRecorder::cob_cost(hardware::COBCoord coord, std::usize index, bool reuse_type) const -> float {
    return this->_cob_recorders.at(coord).cob_cost(index, reuse_type);
}

}

