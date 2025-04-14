#pragma once

#include "./tobrecorder.hh"
#include "./cobrecorder.hh"
#include <hardware/interposer.hh>


namespace kiwi::circuit {
    class PathPackage;
}


namespace kiwi::algo {

class HardwareRecorder {
    // a recorder is generated only when it is used
public:
    HardwareRecorder(hardware::Interposer* interposer): _track_recorders{}, _tob_recorders{}, _interposer{interposer} {}
    ~HardwareRecorder() = default;

public:
    auto get_track_recorder(hardware::Track* track, bool reuse_type) -> TypeRecorder&;
    auto get_track_recorder(hardware::Track* track, bool reuse_type) const -> const TypeRecorder&;
    auto get_tob_recorder(hardware::TOBCoord coord) -> TOBRecorder&;
    // auto get_cob_recorder(hardware::COBCoord coord) -> COBRecorder&;
    // auto get_cob_type_recorder(hardware::COBCoord, std::usize, std::usize) -> TypeRecorder&;

    auto update_recorders(const circuit::PathPackage& package, bool reuse_type) -> void;
    auto update_track_recorders(const std::Vector<hardware::Track*>& tracks, bool reuse_type) -> void;
    auto update_tob_recorders(const std::HashMap<hardware::TOBCoord, hardware::TOBConnector>& connectors, bool reuse_type) -> void;
    // auto update_cob_recorders(const std::Vector<hardware::COBConnector>& cob_connectors, bool reuse_type) -> void;
    auto clear_shared(const circuit::PathPackage& package) -> void;

    auto bump_to_track_cost(hardware::TOBCoord, std::usize, hardware::Track*, bool) const -> float;
    auto track_cost(hardware::Track*, bool) const -> float;
    // auto cob_cost(hardware::COBCoord, std::usize, bool) const -> float;

    auto check_shared() const -> bool;
    
private:
    std::HashMap<hardware::Track*, TypeRecorder> _track_recorders;
    std::HashMap<hardware::TOBCoord, TOBRecorder> _tob_recorders;
    // std::HashMap<hardware::COBCoord, COBRecorder> _cob_recorders;
    hardware::Interposer* _interposer;
};

}



