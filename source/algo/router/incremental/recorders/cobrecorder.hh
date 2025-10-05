#pragma once

#include "./recorder.hh"
#include <ranges>
#include <hardware/interposer.hh>


namespace kiwi::algo {

class COBUnitRecorder {
public:
    COBUnitRecorder(std::usize size, bool use_cost);
    ~COBUnitRecorder() = default;

public:
    auto recorder(std::usize index) -> TypeRecorder&;
    auto recorders() -> std::Vector<TypeRecorder>&;
    auto cobunit_cost(std::usize index) const -> float;
    auto unit_info() const -> std::Pair<float, float>;
    auto re_initialize() -> void;
    auto clear_history_record(std::usize index) -> void;

private:
    std::usize _size;
    std::Vector<TypeRecorder> _cobunit_recorder;
};

class COBRecorder {
public:
    COBRecorder(bool use_cost);
    ~COBRecorder() = default;

public:
    auto unit_recorder(std::usize index) -> COBUnitRecorder&;
    auto cob_cost(std::usize index, bool reuse_type) const -> float;
    auto parse_index(std::usize index) const -> std::Tuple<std::usize, std::usize>;
    auto group_info(std::usize index) const -> std::Tuple<std::usize, std::usize>;
    auto update_type(std::usize index, bool reuse_type) -> void;
    auto update_history(std::usize index, bool reuse_type) -> void;
    auto update_cost(std::usize index) -> void;
    auto re_initialize() -> void;
    auto clear_history_record(std::usize index) -> void;

private:
    std::Vector<COBUnitRecorder> _cob_recorder;
};

}


