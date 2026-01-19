#include "./cobrecorder.hh"
#include <stdexcept>


namespace kiwi::algo {

COBUnitRecorder::COBUnitRecorder(std::usize size, bool use_cost) : _size(size) {
    this->_cobunit_recorder.reserve(4);
    this->_cobunit_recorder.emplace(hardware::COBDirection::Up, std::vector<TypeRecorder>{});
    this->_cobunit_recorder.emplace(hardware::COBDirection::Down, std::vector<TypeRecorder>{});
    this->_cobunit_recorder.emplace(hardware::COBDirection::Left, std::vector<TypeRecorder>{});
    this->_cobunit_recorder.emplace(hardware::COBDirection::Right, std::vector<TypeRecorder>{});

    for (auto& [dir, recorders]: this->_cobunit_recorder) {
        recorders.reserve(size);
        for (auto i: std::views::iota(0, (int)size)) {
            recorders.emplace_back(TypeRecorder{use_cost});
        }
    }
}

auto COBUnitRecorder::recorder(hardware::COBDirection dir, std::usize index) -> TypeRecorder& {
    if (index >= this->_size) {
        throw std::out_of_range("COBUnitRecorder::recorder(): index out of range");
    }
    return this->_cobunit_recorder.at(dir).at(index);
}

auto COBUnitRecorder::cobunit_cost(hardware::COBDirection dir, std::usize index, bool reuse_type) const -> float {
try {
    return this->_cobunit_recorder.at(dir).at(index).cost(reuse_type);
}
catch (std::exception& e) {
    throw std::runtime_error(std::format("cobunit_cost(): at index = {}, _cobunit vector size = , {}", index, this->_cobunit_recorder.size(), e.what()));
}
}

auto COBUnitRecorder::unit_info(hardware::COBDirection dir) const -> std::Pair<float, float> {
    float reuse_num{0.0}, nonre_num{0.0};
        
    for (auto& recorder: this->_cobunit_recorder.at(dir)) {
        if (recorder.current_type().has_value()) {
            recorder.current_type().value() ? reuse_num++ : nonre_num++;
        }
    }

    return std::Pair<float, float>{reuse_num, nonre_num};
}

auto COBUnitRecorder::re_initialize() -> void {
    for (auto& [dir, recorders]: this->_cobunit_recorder) {
        for (auto& unit_recorder: recorders) {
            unit_recorder.re_initialize();
        }
    }
}

auto COBUnitRecorder::recorders(hardware::COBDirection dir) -> std::Vector<TypeRecorder>& {
    return this->_cobunit_recorder.at(dir);
}

auto COBUnitRecorder::clear_history_record(hardware::COBDirection dir, std::usize index) -> void {
    if (index >= this->_size) {
        throw std::out_of_range("COBUnitRecorder::recorder(): index out of range");
    }

    this->_cobunit_recorder.at(dir).at(index).reset_type();
    this->_cobunit_recorder.at(dir).at(index).update_cost(0, 0);
}

auto COBUnitRecorder::show_data(hardware::COBDirection dir, std::tuple<std::size_t, std::size_t> unit_info, bool show_all, bool print) const -> std::string {
    std::string msg = "";

    if (show_all) {
        auto [unit, _] = unit_info;
        for (std::usize i = 0; i < this->_size; ++i) {
            msg += std::format("{}: {}", unit * hardware::COBUnit::WILTON_SIZE + i, this->_cobunit_recorder.at(dir).at(i).show_data(print));
        }
    }
    else {
        auto [unit, index_in_unit] = unit_info;
        msg += std::format("{}: {}", unit * hardware::COBUnit::WILTON_SIZE + index_in_unit, this->_cobunit_recorder.at(dir).at(index_in_unit).show_data(print));
    }

    if (print) {
        debug::info(msg);
    }
    return msg;
}

auto COBUnitRecorder::self_cost() const -> std::tuple<float, float> {
    auto total_cost = std::tuple<float, float>(0.0, 0.0);
    for (auto& [dir, recorders]: this->_cobunit_recorder) {
        for (auto& recorder: recorders) {
            auto [cost_reuse, cost_nonreuse] = recorder.self_cost();
            total_cost = std::tuple<float, float>(std::get<0>(total_cost) + cost_reuse, std::get<1>(total_cost) + cost_nonreuse);
        }
    }

    return total_cost;
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

auto COBRecorder::unit_recorder(std::usize index) const -> const COBUnitRecorder& {
    return const_cast<COBRecorder*>(this)->unit_recorder(index);
}

auto COBRecorder::cob_cost(hardware::COBDirection dir, std::usize index, bool reuse_type) const -> float {
    return this->_cob_recorder.at(index/hardware::COB::UNIT_SIZE).cobunit_cost(dir, index%hardware::COBUnit::WILTON_SIZE, reuse_type);
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

auto COBRecorder::group_info(hardware::COBDirection dir, std::usize index) const -> std::Tuple<float, float> {
try {
    float reuse_num{0.0}, nonre_num{0.0};
    std::usize group_index = index / TRACKGROUPSIZE;
    for (auto i: std::views::iota(0, 4)) {
        auto unit_recorder = this->_cob_recorder.at(group_index * 4 + i);
        auto [reuse, nonre] = unit_recorder.unit_info(dir);
        reuse_num += reuse;
        nonre_num += nonre;
    }
    return std::Tuple<float, float>{reuse_num, nonre_num};
}
catch (std::exception& e) {
    throw std::runtime_error(std::format("cob_cost(): in unit {}, _cob_vector size = {}, -> {}", index/hardware::COB::UNIT_SIZE, this->_cob_recorder.size(), e.what()));
}
}

auto COBRecorder::update_type(hardware::COBDirection dir, std::usize index, bool reuse_type) -> void {
    if (index >= hardware::TOB::INDEX_SIZE) {
        throw std::out_of_range("COBRecorder::unit_recorder(): index out of range");
    }

    auto [unit, index_in_unit] = this->parse_index(index);
    this->_cob_recorder.at(unit).recorder(dir, index_in_unit).set_type(reuse_type);
}

auto COBRecorder::update_history(hardware::COBDirection dir, std::usize index, bool reuse_type) -> void {
    if (index >= hardware::TOB::INDEX_SIZE) {
        throw std::out_of_range("COBRecorder::unit_recorder(): index out of range");
    }

    auto [unit, index_in_unit] = this->parse_index(index);
    this->_cob_recorder.at(unit).recorder(dir, index_in_unit).update_history(reuse_type);
}

auto COBRecorder::update_cost(hardware::COBDirection dir, std::usize index) -> void {
    if (index >= hardware::TOB::INDEX_SIZE) {
        throw std::out_of_range("COBRecorder::unit_recorder(): index out of range");
    }

    auto [unit, index_in_unit] = this->parse_index(index);
    auto cob_inner_index = unit * hardware::COBUnit::WILTON_SIZE + index_in_unit;

    auto [reuse_num, nonre_num] = this->group_info(dir, cob_inner_index);
    std::usize group_index = cob_inner_index / TRACKGROUPSIZE;
    for (auto i: std::views::iota(0, 4)) {
        auto& unit_recorder = this->_cob_recorder.at(group_index * 4 + i);
        for (auto& recorder: unit_recorder.recorders(dir)) {
            recorder.update_cost(reuse_num, nonre_num);
        }
    }
}

auto COBRecorder::re_initialize() -> void {
    for (auto& recorder: this->_cob_recorder) {
        recorder.re_initialize();
    }
}

auto COBRecorder::clear_history_record(hardware::COBDirection dir, std::usize index) -> void {
    auto [unit, index_in_unit] = this->parse_index(index);
    this->_cob_recorder.at(unit).clear_history_record(dir, index_in_unit);
}

auto COBRecorder::show_data(hardware::COBDirection dir, std::size_t track_index, bool show_all, bool print) const -> std::string {
    std::string msg = "";
    auto [cobunit, index_in_unit] = this->parse_index(track_index);

    if (show_all) {
        auto group_index = cobunit / 4;
        for (auto i: std::views::iota(0, 4)) {
            auto unit_index = group_index * 4 + i;
            const auto& unit_recorder = this->_cob_recorder.at(unit_index);
            msg += unit_recorder.show_data(dir, std::make_tuple(unit_index, index_in_unit), show_all, print);
        }
    }
    else{
        const auto& unit_recorder = this->_cob_recorder.at(cobunit);
        msg += unit_recorder.show_data(dir, std::make_tuple(cobunit, index_in_unit), show_all, print);
    }
    

    if (print) {
        debug::info(msg);
    }
    return msg;
}

auto COBRecorder::self_cost() const -> std::tuple<float, float> {
    auto total_cost = std::tuple<float, float>(0.0, 0.0);
    for (auto& recorder: this->_cob_recorder) {
        auto [cost_reuse, cost_nonreuse] = recorder.self_cost();
        total_cost = std::tuple<float, float>(std::get<0>(total_cost) + cost_reuse, std::get<1>(total_cost) + cost_nonreuse);
    }
    return total_cost;
}

}


