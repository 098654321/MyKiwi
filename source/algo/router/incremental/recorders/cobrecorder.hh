#pragma once

#include "./recorder.hh"
#include <ranges>
#include <hardware/interposer.hh>
#include <unordered_map>
#include <hardware/cob/cobdirection.hh>


namespace kiwi::algo {

class COBUnitRecorder {
public:
    COBUnitRecorder(std::usize size, bool use_cost);
    ~COBUnitRecorder() = default;

public:
    auto recorder(hardware::COBDirection, std::usize index) -> TypeRecorder&;
    auto recorders(hardware::COBDirection) -> std::Vector<TypeRecorder>&;
    auto cobunit_cost(hardware::COBDirection dir, std::usize index, bool reuse_type) const -> float;
    auto unit_info(hardware::COBDirection) const -> std::Pair<float, float>;
    auto re_initialize() -> void;
    auto clear_history_record(hardware::COBDirection dir, std::usize index) -> void;
    auto show_data(hardware::COBDirection dir, std::tuple<std::size_t, std::size_t> unit_info, bool show_all, bool print = false) const -> std::string;

private:
    std::usize _size;
    std::unordered_map<hardware::COBDirection, std::vector<TypeRecorder>> _cobunit_recorder;
};

class COBRecorder {
public:
    COBRecorder(bool use_cost);
    ~COBRecorder() = default;

public:
    auto unit_recorder(std::usize index) -> COBUnitRecorder&;
    auto unit_recorder(std::usize index) const -> const COBUnitRecorder&;
    auto cob_cost(hardware::COBDirection dir, std::usize index, bool reuse_type) const -> float;
    auto parse_index(std::usize index) const -> std::Tuple<std::usize, std::usize>;
    auto group_info(hardware::COBDirection, std::usize index) const -> std::Tuple<float, float>;
    auto update_type(hardware::COBDirection dir, std::usize index, bool reuse_type) -> void;
    auto update_history(hardware::COBDirection dir, std::usize index, bool reuse_type) -> void;
    auto update_cost(hardware::COBDirection, std::usize index) -> void;
    auto re_initialize() -> void;
    auto clear_history_record(hardware::COBDirection dir, std::usize index) -> void;
    auto show_data(hardware::COBDirection dir, std::size_t track_index, bool show_all, bool print = false) const -> std::string;

private:
    std::Vector<COBUnitRecorder> _cob_recorder;
};

}


