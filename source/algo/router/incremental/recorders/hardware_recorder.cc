#include "./hardware_recorder.hh"
#include <circuit/path/pathpackage.hh>
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

// auto HardwareRecorder::get_cob_recorder(hardware::COBCoord coord) -> COBRecorder& {
//     auto iter = this->_cob_recorders.emplace(coord, COBRecorder{});
//     return iter.first->second;
// }

// auto HardwareRecorder::get_cob_type_recorder(
//     hardware::COBCoord coord, std::usize unit_i, std::usize reg_i
// ) -> TypeRecorder& {
//     auto unit_recorder = this->get_cob_recorder(coord).unit_recorder(unit_i);
//     return unit_recorder.recorder(reg_i);
// }

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

// auto HardwareRecorder::cob_cost(hardware::COBCoord coord, std::usize index, bool reuse_type) const -> float {
//     return this->_cob_recorders.at(coord).cob_cost(index, reuse_type);
// }

auto HardwareRecorder::update_recorders(const circuit::PathPackage& package, bool reuse_type) -> void {
    std::Vector<hardware::Track*> tracks {};
    for (auto& [track, connector]: package._regular_path) {
        tracks.emplace_back(track);
    }
    this->update_track_recorders(tracks, reuse_type);

    std::HashMap<hardware::TOBCoord, hardware::TOBConnector> tobconnectors {};
    for (auto& [bump, connector, track]: package._tob_to_track) {
        tobconnectors.emplace(bump->tob()->coord(), connector);
    }
    for (auto& [bump, connector, track]: package._track_to_tob) {
        tobconnectors.emplace(bump->tob()->coord(), connector);
    }
    this->update_tob_recorders(tobconnectors, reuse_type);
}

auto HardwareRecorder::update_track_recorders(const std::Vector<hardware::Track*>& tracks, bool reuse_type) -> void {
    for (auto& track: tracks) {
        auto& recorder = this->get_track_recorder(track, reuse_type);
        recorder.update(reuse_type);
    }
}   

auto HardwareRecorder::update_tob_recorders(const std::HashMap<hardware::TOBCoord, hardware::TOBConnector>& connectors, bool reuse_type) -> void {
    for (auto& [coord, connector]: connectors) {
        this->_tob_recorders.at(coord).update(connector.bump_index(), connector.hori_index(), connector.vert_index(), reuse_type);
    }
}

auto HardwareRecorder::clear_shared(const circuit::PathPackage& package) -> void {
    for (auto& [bump, connector, track]: package._tob_to_track) {
        this->_tob_recorders.at(bump->tob()->coord()).clear_mux_shared(connector.bump_index(), connector.hori_index(), connector.vert_index());
    }
    for (auto& [bump, connector, track]: package._track_to_tob) {
        this->_tob_recorders.at(bump->tob()->coord()).clear_mux_shared(connector.bump_index(), connector.hori_index(), connector.vert_index());
    }
}

auto HardwareRecorder::check_shared() const -> bool {
    for (auto& [coord, tobrecorder]: this->_tob_recorders) {
        if (tobrecorder.check_shared()) {
            return true;
        }
    }
    return false;
}

}

