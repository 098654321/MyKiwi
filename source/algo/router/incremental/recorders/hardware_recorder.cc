#include "./hardware_recorder.hh"
#include <debug/debug.hh>
#include <ranges>
#include <algo/router/routeerror.hh>


namespace kiwi::algo{

auto HardwareRecorder::get_track_recorder(hardware::Track* track, bool reuse_type) -> TypeRecorder& {
    auto iter = this->_track_recorders.emplace(track, TypeRecorder{reuse_type, this->_use_cost});
    return iter.first->second;
}

auto HardwareRecorder::get_track_recorder(hardware::Track* track, bool reuse_type) const -> const TypeRecorder& {
    return const_cast<HardwareRecorder*>(this)->get_track_recorder(track, reuse_type);
}

auto HardwareRecorder::get_tob_recorder(hardware::TOBCoord coord) -> TOBRecorder& {
    auto iter = this->_tob_recorders.emplace(coord, TOBRecorder{this->_use_cost});
    return iter.first->second;
}

auto HardwareRecorder::get_cob_recorder(hardware::COBCoord coord) -> COBRecorder& {
    auto iter = this->_cob_recorders.emplace(coord, COBRecorder{this->_use_cost});
    return iter.first->second;
}

auto HardwareRecorder::get_cob_type_recorder(
    hardware::COBCoord coord, std::usize unit_i, std::usize reg_i
) -> TypeRecorder& {
    auto unit_recorder = this->get_cob_recorder(coord).unit_recorder(unit_i);
    return unit_recorder.recorder(reg_i);
}

auto HardwareRecorder::bump_to_track_cost(hardware::TOBCoord coord, std::usize bump_index, hardware::Track* track, bool reuse_type) -> float {
    auto track_index = track->coord().index;
    return this->get_tob_recorder(coord).tob_cost(bump_index, track_index, reuse_type) + this->track_cost(track,reuse_type);
}

auto HardwareRecorder::cob_cost(hardware::COBCoord coord, std::usize index, bool reuse_type) -> float {
try {
    return this->get_cob_recorder(coord).cob_cost(index, reuse_type);
}
catch(std::exception& e) {
    throw FinalError(std::format("cob_cost(): {}", e.what()));
}
}

auto HardwareRecorder::track_cost(hardware::Track* track, bool reuse_type) -> float {
    auto& type_recorder = this->get_track_recorder(track, reuse_type);
    return type_recorder.cost(reuse_type);
}

auto HardwareRecorder::expand_cost(hardware::Track* track, hardware::COBConnector& connector, bool reuse_type) -> float {
try {
    auto cob_cost = this->cob_cost(connector.coord(), connector.from_track_index(), reuse_type);
    auto track_cost = this->track_cost(track, reuse_type);
    return cob_cost + track_cost;
}
catch (std::exception& e) {
    throw FinalError(std::format("HardwareRecorder::expand_cost(): {}", e.what()));
}
}

auto HardwareRecorder::update_recorders_current(const circuit::PathPackage& package, bool reuse_type) -> void  {
    debug::info("Update recorders current");
    std::Vector<hardware::Track*> tracks {};
    std::Vector<hardware::COBConnector> cobconnectors {};
    for (auto& [track, connector]: package._regular_path) {
        tracks.emplace_back(track);
        if (connector.has_value()) {
            cobconnectors.emplace_back(*connector);
        }
    }
    this->update_track_recorders_current(tracks, reuse_type);
    this->update_cob_recorders_current(cobconnectors, reuse_type);

    std::HashMap<hardware::TOBCoord, hardware::TOBConnector> tobconnectors {};
    for (auto& [bump, connector, track]: package._tob_to_track) {
        tobconnectors.emplace(bump->tob()->coord(), connector);
    }
    for (auto& [bump, connector, track]: package._track_to_tob) {
        tobconnectors.emplace(bump->tob()->coord(), connector);
    }
    this->update_tob_recorders_current(tobconnectors, reuse_type);
}

auto HardwareRecorder::update_recorders_history(const circuit::PathPackage& package, bool reuse_type) -> void {
    debug::info("Update recorders history");
    std::Vector<hardware::Track*> tracks {};
    std::Vector<hardware::COBConnector> cobconnectors {};
    for (auto& [track, connector]: package._regular_path) {
        tracks.emplace_back(track);
        if (connector.has_value()) {
            cobconnectors.emplace_back(*connector);
        }
    }
    this->update_track_recorders_history(tracks, reuse_type);
    this->update_cob_recorders_history(cobconnectors, reuse_type);

    std::HashMap<hardware::TOBCoord, hardware::TOBConnector> tobconnectors {};
    for (auto& [bump, connector, track]: package._tob_to_track) {
        tobconnectors.emplace(bump->tob()->coord(), connector);
    }
    for (auto& [bump, connector, track]: package._track_to_tob) {
        tobconnectors.emplace(bump->tob()->coord(), connector);
    }
    this->update_tob_recorders_history(tobconnectors, reuse_type);
}

auto HardwareRecorder::update_cob_recorders_current(const std::Vector<hardware::COBConnector>& cob_connectors, bool reuse_type) -> void {
    for (auto& connector: cob_connectors) {
        auto coord = connector.coord();
        this->get_cob_recorder(coord).update_type(connector.from_track_index(), reuse_type);
    }
    for (auto& connector: cob_connectors) {
        auto coord = connector.coord();
        this->get_cob_recorder(coord).update_cost(connector.from_track_index());
    }
}

auto HardwareRecorder::update_cob_recorders_history(const std::Vector<hardware::COBConnector>& cob_connectors, bool reuse_type) -> void {
    for (auto& connector: cob_connectors) {
        auto coord = connector.coord();
        this->get_cob_recorder(coord).update_history(connector.from_track_index(), reuse_type);
    }
    for (auto& connector: cob_connectors) {
        auto coord = connector.coord();
        this->get_cob_recorder(coord).update_cost(connector.from_track_index());
    }
}


auto HardwareRecorder::update_track_recorders_cost(const std::Vector<hardware::Track*>& tracks, bool reuse_type) -> void {
    std::unordered_map<hardware::TrackCoord, std::unordered_map<int, std::Pair<float, float>>> group_info {};

    // collect group info
    for (auto& track: tracks) {
        auto track_coord = track->coord();
        int group_index = track_coord.index / TRACKGROUPSIZE;

        if (group_info.contains(track_coord) && group_info.at(track_coord).contains(group_index)) {
            continue;
        }
        else {
            auto reuse_num=0, nonre_num= 0;
            for (auto i: std::views::iota((int)(TRACKGROUPSIZE*group_index), (int)(TRACKGROUPSIZE*group_index+TRACKGROUPSIZE-1))) {
                auto coord = hardware::TrackCoord(track_coord.row, track_coord.col, track_coord.dir, i);
                auto t = this->_interposer->get_track(coord);

                if(t.has_value() && this->_track_recorders.contains(t.value())) {
                    auto track_recorder = this->_track_recorders.at(t.value());

                    if(track_recorder.current_type().has_value()){
                        auto track_type = *track_recorder.current_type();
                        track_type ? reuse_num++ : nonre_num++;
                    }
                }
            }

            auto iter = group_info.emplace(track_coord, std::unordered_map<int, std::Pair<float, float>>{});
            iter.first->second.emplace(group_index, std::make_pair(reuse_num, nonre_num));
        }
    }

    // update all groups
    for (const auto& [track_coord, info]: group_info) {
        for (const auto& [group, use_info]: info) {
            for (auto i: std::views::iota((int)(TRACKGROUPSIZE*group), (int)(TRACKGROUPSIZE*group+TRACKGROUPSIZE-1))) {
                auto coord = hardware::TrackCoord(track_coord.row, track_coord.col, track_coord.dir, i);
                auto t = this->_interposer->get_track(coord);
                
                if(t.has_value()) {
                    auto& track_recorder = this->get_track_recorder(t.value(), reuse_type);
                    track_recorder.update_cost(use_info.first, use_info.second);
                }
            }
        }
        
    }
}


auto HardwareRecorder::update_track_recorders_current(const std::Vector<hardware::Track*>& tracks, bool reuse_type) -> void {
    // set type
    for (auto& track: tracks) {
        auto& recorder = this->get_track_recorder(track, reuse_type);
        recorder.set_type(reuse_type);
    }

    // update cost in group
    this->update_track_recorders_cost(tracks, reuse_type);
}

auto HardwareRecorder::update_track_recorders_history(const std::Vector<hardware::Track*>& tracks, bool reuse_type) -> void {
    // update history
    for (auto& track: tracks) {
        auto& recorder = this->get_track_recorder(track, reuse_type);
        recorder.update_history(reuse_type);
    }

    // update cost in group
    this->update_track_recorders_cost(tracks, reuse_type);
}

auto HardwareRecorder::update_tob_recorders_current(const std::HashMap<hardware::TOBCoord, hardware::TOBConnector>& connectors, bool reuse_type) -> void {
    for (auto iter = connectors.begin(); iter != connectors.end(); ++iter) {
        auto& [coord, connector] = *iter;
        auto& recorder = this->get_tob_recorder(coord);
        recorder.update_type(connector.bump_index(), connector.hori_index(), connector.vert_index(), reuse_type);
    }
    for (auto iter = connectors.begin(); iter != connectors.end(); ++iter) {
        auto& [coord, connector] = *iter;
        auto& recorder = this->get_tob_recorder(coord);
        recorder.update_cost(connector.bump_index(), connector.hori_index(), connector.vert_index(), reuse_type);
    }
}

auto HardwareRecorder::update_tob_recorders_history(const std::HashMap<hardware::TOBCoord, hardware::TOBConnector>& connectors, bool reuse_type) -> void {
    for (auto iter = connectors.begin(); iter != connectors.end(); ++iter) {
        auto& [coord, connector] = *iter;
        auto& recorder = this->get_tob_recorder(coord);
        recorder.update_history(connector.bump_index(), connector.hori_index(), connector.vert_index(), reuse_type);
    }
    for (auto iter = connectors.begin(); iter != connectors.end(); ++iter) {
        auto& [coord, connector] = *iter;
        auto& recorder = this->get_tob_recorder(coord);
        recorder.update_cost(connector.bump_index(), connector.hori_index(), connector.vert_index(), reuse_type);
    }
}

auto HardwareRecorder::clear_history_records(const circuit::PathPackage& package, bool reuse_type) -> void {
    std::Vector<hardware::Track*> tracks {};
    std::Vector<hardware::COBConnector> cobconnectors {};
    for (auto& [track, connector]: package._regular_path) {
        tracks.emplace_back(track);
        if (connector.has_value()) {
            cobconnectors.emplace_back(*connector);
        }
    }
    this->clear_track_history_records(tracks, reuse_type);
    this->clear_cob_history_records(cobconnectors, reuse_type);

    std::HashMap<hardware::TOBCoord, hardware::TOBConnector> tobconnectors {};
    for (auto& [bump, connector, track]: package._tob_to_track) {
        tobconnectors.emplace(bump->tob()->coord(), connector);
    }
    for (auto& [bump, connector, track]: package._track_to_tob) {
        tobconnectors.emplace(bump->tob()->coord(), connector);
    }
    this->clear_tob_history_records(tobconnectors, reuse_type);
}

auto HardwareRecorder::clear_track_history_records(const std::Vector<hardware::Track*>& tracks, bool reuse_type) -> void {
    for (auto& track: tracks) {
        auto& recorder = this->get_track_recorder(track, reuse_type);
        recorder.reset_type();
        recorder.update_cost(0, 0);
    }
}

auto HardwareRecorder::clear_cob_history_records(const std::Vector<hardware::COBConnector>& cob_connectors, bool reuse_type) -> void {
    for (auto& connector: cob_connectors) {
        auto coord = connector.coord();
        auto& recorder = this->get_cob_recorder(coord);
        recorder.clear_history_record(connector.from_track_index());
    }
}

auto HardwareRecorder::clear_tob_history_records(const std::HashMap<hardware::TOBCoord, hardware::TOBConnector>& connectors, bool reuse_type) -> void {
    for(auto& [coord, connector]: connectors) {
        auto& recorder = this->get_tob_recorder(coord);
        recorder.clear_history_record(connector.bump_index(), connector.hori_index(), connector.vert_index());
    }
}

auto HardwareRecorder::re_initialize() -> void {
    for (auto& [track, recorder]: this->_track_recorders) {
        recorder.re_initialize();
    }
    for (auto& [coord, recorder]: this->_tob_recorders) {
        recorder.re_initialize();
    }
    for (auto& [coord, recorder]: this->_cob_recorders) {
        recorder.re_initialize();
    }
}

auto HardwareRecorder::set_use_cost(bool use_cost) -> void {
    this->_use_cost = use_cost;
}

}

