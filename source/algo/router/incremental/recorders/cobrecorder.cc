#include "./cobrecorder.hh"
#include <stdexcept>


namespace kiwi::algo {

COBUnitRecorder::COBUnitRecorder(std::usize size, bool use_cost) : _size(size) {
    this->_cobunit_recorder.reserve(size);
    for (auto i: std::views::iota(0, (int)size)) {
        this->_cobunit_recorder.emplace_back(TypeRecorder{use_cost});
    }
}

auto COBUnitRecorder::recorder(std::usize index) -> TypeRecorder& {
    if (index >= this->_size) {
        throw std::out_of_range("COBUnitRecorder::recorder(): index out of range");
    }
    return this->_cobunit_recorder.at(index);
}

auto COBUnitRecorder::cobunit_cost(std::usize index) const -> float {
try {
    return this->_cobunit_recorder.at(index).cost();
}
catch (std::exception& e) {
    throw std::runtime_error(std::format("cobunit_cost(): at index = {}, _cobunit vector size = , {}", index, this->_cobunit_recorder.size(), e.what()));
}
}

auto COBUnitRecorder::unit_info() const -> std::Pair<float, float> {
    std::usize reuse_num{0}, nonre_num{0};
    for (auto& recorder: this->_cobunit_recorder) {
        if (recorder.current_type().has_value()) {
            recorder.current_type().value() ? reuse_num++ : nonre_num++;
        }
    }
    return std::Pair<std::usize, std::usize>{reuse_num, nonre_num};
}

auto COBUnitRecorder::re_initialize() -> void {
    for (auto& recorder: this->_cobunit_recorder) {
        recorder.re_initialize();
    }
}

auto COBUnitRecorder::recorders() -> std::Vector<TypeRecorder>& {
    return this->_cobunit_recorder;
}

auto COBUnitRecorder::clear_history_record(std::usize index) -> void {
    if (index >= this->_size) {
        throw std::out_of_range("COBUnitRecorder::recorder(): index out of range");
    }

    this->_cobunit_recorder.at(index).reset_type();
}

COBRecorder::COBRecorder(bool use_cost) {
    this->_cob_recorder.reserve(hardware::COB::UNIT_SIZE);
    for (auto i: std::views::iota(0, (int)hardware::COB::UNIT_SIZE)) {
        this->_cob_recorder.emplace_back(COBUnitRecorder{hardware::COBUnit::WILTON_SIZE, use_cost});
    }
}

auto COBRecorder::unit_recorder(std::usize index) -> COBUnitRecorder& {
    if (index >= hardware::COB::UNIT_SIZE) {
        throw std::out_of_range("COBRecorder::unit_recorder(): index out of range");
    }
    return this->_cob_recorder.at(index);
}

auto COBRecorder::cob_cost(std::usize index, bool reuse_type) const -> float {
    return this->_cob_recorder.at(index/hardware::COB::UNIT_SIZE).cobunit_cost(index%hardware::COBUnit::WILTON_SIZE);
}

// return [cobunit, index_in_unit]
auto COBRecorder::parse_index(std::usize index) const -> std::Tuple<std::usize, std::usize> {
    if (index >= hardware::TOB::INDEX_SIZE) {
        throw std::out_of_range("COBRecorder::unit_recorder(): index out of range");
    }

    if (index < hardware::TOB::INDEX_SIZE/2) {
        return std::Tuple<std::usize, std::usize> {
            index % hardware::COBUnit::WILTON_SIZE,
            index / hardware::COBUnit::WILTON_SIZE,
        };
    }
    else {
        index = index - hardware::TOB::INDEX_SIZE/2;
        return std::Tuple<std::usize, std::usize> {
            index % hardware::COBUnit::WILTON_SIZE + hardware::COB::UNIT_SIZE/2,
            index / hardware::COBUnit::WILTON_SIZE,
        };
    }
}

auto COBRecorder::group_info(std::usize index) const -> std::Tuple<std::usize, std::usize> {
try {
    std::usize reuse_num{0}, nonre_num{0};
    std::usize group_index = index / TRACKGROUPSIZE;
    for (auto i: std::views::iota(0, 4)) {
        auto unit_recorder = this->_cob_recorder.at(group_index * 4 + i);
        auto [reuse, nonre] = unit_recorder.unit_info();
        reuse_num += reuse;
        nonre_num += nonre;
    }
}
catch (std::exception& e) {
    throw std::runtime_error(std::format("cob_cost(): in unit {}, _cob_vector size = {}, -> {}", index/hardware::COB::UNIT_SIZE, this->_cob_recorder.size(), e.what()));
}
}

auto COBRecorder::update_type(std::usize index, bool reuse_type) -> void {
    if (index >= hardware::TOB::INDEX_SIZE) {
        throw std::out_of_range("COBRecorder::unit_recorder(): index out of range");
    }

    auto [unit, index_in_unit] = this->parse_index(index);
    this->_cob_recorder.at(unit).recorder(index_in_unit).set_type(reuse_type);
}

auto COBRecorder::update_history(std::usize index, bool reuse_type) -> void {
    if (index >= hardware::TOB::INDEX_SIZE) {
        throw std::out_of_range("COBRecorder::unit_recorder(): index out of range");
    }

    auto [unit, index_in_unit] = this->parse_index(index);
    this->_cob_recorder.at(unit).recorder(index_in_unit).update_history(reuse_type);
}

auto COBRecorder::update_cost(std::usize index) -> void {
    if (index >= hardware::TOB::INDEX_SIZE) {
        throw std::out_of_range("COBRecorder::unit_recorder(): index out of range");
    }

    auto [reuse_num, nonre_num] = this->group_info(index);
    std::usize group_index = index / TRACKGROUPSIZE;
    for (auto i: std::views::iota(0, 4)) {
        auto unit_recorder = this->_cob_recorder.at(group_index * 4 + i);
        for (auto& recorder: unit_recorder.recorders()) {
            recorder.update_cost(reuse_num, nonre_num);
        }
    }
}

auto COBRecorder::re_initialize() -> void {
    for (auto& recorder: this->_cob_recorder) {
        recorder.re_initialize();
    }
}

auto COBRecorder::clear_history_record(std::usize index) -> void {
    auto [unit, index_in_unit] = this->parse_index(index);
    this->_cob_recorder.at(unit).clear_history_record(index_in_unit);
}

}


