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
    auto cobunit_cost(std::usize index, bool reuse_type) const -> float;
    auto unit_info() const -> std::Pair<float, float>;
    auto re_initialize() -> void;
    auto clear_history_record(std::usize index) -> void;
    auto show_data(std::tuple<std::size_t, std::size_t> unit_info, bool show_all, bool print = false) const -> std::string;

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
    auto unit_recorder(std::usize index) const -> const COBUnitRecorder&;
    auto cob_cost(std::usize index, bool reuse_type) const -> float;
    auto parse_index(std::usize index) const -> std::Tuple<std::usize, std::usize>;
    auto group_info(std::usize index) const -> std::Tuple<std::usize, std::usize>;
    auto update_type(std::usize index, bool reuse_type) -> void;
    auto update_history(std::usize index, bool reuse_type) -> void;
    auto update_cost(std::usize index) -> void;
    auto re_initialize() -> void;
    auto clear_history_record(std::usize index) -> void;
    auto show_data(std::size_t track_index, bool show_all, bool print = false) const -> std::string;

private:
    std::Vector<COBUnitRecorder> _cob_recorder;
};

}


