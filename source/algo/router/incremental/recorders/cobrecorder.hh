#pragma once

#include "./recorder.hh"
#include <ranges>
#include <hardware/interposer.hh>


namespace kiwi::algo {

class COBUnitRecorder {
public:
    COBUnitRecorder(std::usize size);
    ~COBUnitRecorder() = default;

public:
    auto recorder(std::usize index) -> TypeRecorder&;
    auto cobunit_cost(std::usize index, std::usize reuse_num, std::usize nonre_num) const -> float;
    auto unit_info() const -> std::Pair<std::usize, std::usize>;

private:
    std::usize _size;
    std::Vector<TypeRecorder> _cobunit_recorder;
};

class COBRecorder {
public:
    COBRecorder();
    ~COBRecorder() = default;

public:
    auto unit_recorder(std::usize index) -> COBUnitRecorder&;
    auto cob_cost(std::usize index, bool reuse_type) const -> float;
    auto parse_index(std::usize index) const -> std::Tuple<std::usize, std::usize>;
    auto update(std::usize index, bool reuse_type) -> void;

private:
    std::Vector<COBUnitRecorder> _cob_recorder;
};

}


