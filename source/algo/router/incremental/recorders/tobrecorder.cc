#include "./tobrecorder.hh"
#include <debug/debug.hh>
#include <stdexcept>
#include <format>


namespace kiwi::algo {

TOBMuxRecorder::TOBMuxRecorder(std::usize size, bool use_cost) : _size(size) {
    this->_mux_type_recorder.reserve(size);
    for (auto i: std::views::iota(0, (int)size)) {
        this->_mux_type_recorder.emplace_back(TypeRecorder{use_cost});
    }
}

auto TOBMuxRecorder::type_recorder(std::usize index) -> TypeRecorder& {
    if (index >= this->_size) {
        throw std::out_of_range("TOBMuxRecorder::type_recorder(): index out of range");
    }
    return this->_mux_type_recorder.at(index);
}

auto TOBMuxRecorder::mux_cost(std::usize index, bool reuse_type) const -> float {
    if (index >= this->_size) {
        throw std::out_of_range(std::format("index {} is out of range [0, {})", index, this->_size));
    }
    return this->_mux_type_recorder.at(index).cost(reuse_type);
}

auto TOBMuxRecorder::group_info() const -> std::Pair<float, float> {
    std::usize reuse_num = 0, nonre_num = 0;
    for (auto& recorder: this->_mux_type_recorder) {
        if (recorder.current_type().has_value()) {
            recorder.current_type().value() ? reuse_num++ : nonre_num++;
        } 
    }
    return std::Pair<std::usize, std::usize>{reuse_num, nonre_num};
}

auto TOBMuxRecorder::update_type(std::usize index, bool reuse_type) -> void {
    if (index >= this->_size) {
        throw std::out_of_range(std::format("index {} is out of range [0, {})", index, this->_size));
    }

    this->_mux_type_recorder.at(index).set_type(reuse_type);
}

auto TOBMuxRecorder::update_history(std::usize index, bool reuse_type) -> void {
    if (index >= this->_size) {
        throw std::out_of_range(std::format("index {} is out of range [0, {})", index, this->_size));
    }

    this->_mux_type_recorder.at(index).update_history(reuse_type);
}


auto TOBMuxRecorder::update_cost() -> void {
    auto [reuse_num, nonre_num] = this->group_info();
    for (auto& recorder: this->_mux_type_recorder) {
        recorder.update_cost(reuse_num, nonre_num);
    }
}

auto TOBMuxRecorder::re_initialize() -> void {
    for (auto& recorder: this->_mux_type_recorder) {
        recorder.re_initialize();
    }
}

auto TOBMuxRecorder::clear_history_record(std::usize index) -> void {
    this->_mux_type_recorder.at(index).reset_type();
    this->_mux_type_recorder.at(index).update_cost(0, 0);
}

TOBRecorder::TOBRecorder(bool use_cost) {
    this->_bump_to_hori_recorder.reserve(hardware::TOB::BUMP_TO_HORI_MUX_COUNT);
    for (auto i: std::views::iota(0, (int)hardware::TOB::BUMP_TO_HORI_MUX_COUNT)) {
        this->_bump_to_hori_recorder.emplace_back(TOBMuxRecorder{hardware::TOB::BUMP_TO_HORI_MUX_SIZE, use_cost});
    }

    this->_hori_to_vert_recorder.reserve(hardware::TOB::HORI_TO_VERI_MUX_COUNT);
    for (auto i: std::views::iota(0, (int)hardware::TOB::HORI_TO_VERI_MUX_COUNT)) {
        this->_hori_to_vert_recorder.emplace_back(TOBMuxRecorder{hardware::TOB::HORI_TO_VERI_MUX_SIZE, use_cost});
    }

    this->_vert_to_track_recorder.reserve(hardware::TOB::VERI_TO_TRACK_MUX_COUNT);
    for (auto i: std::views::iota(0, (int)hardware::TOB::VERI_TO_TRACK_MUX_COUNT)) {
        this->_vert_to_track_recorder.emplace_back(TOBMuxRecorder{hardware::TOB::VERI_TO_TRACK_MUX_SIZE, use_cost});
    }
}

auto TOBRecorder::bump_to_hori_recorder(std::usize index) -> TOBMuxRecorder& {
    if (index >= hardware::TOB::BUMP_TO_HORI_MUX_COUNT) {
        throw std::out_of_range("TOBRecorder::bump_to_hori_recorder(): index out of range");
    }
    return this->_bump_to_hori_recorder.at(index);
}

auto TOBRecorder::hori_to_vert_recorder(std::usize index) -> TOBMuxRecorder& {
    if (index >= hardware::TOB::HORI_TO_VERI_MUX_COUNT) {
        throw std::out_of_range("TOBRecorder::hori_to_vert_recorder(): index out of range");
    }
    return this->_hori_to_vert_recorder.at(index);
}

auto TOBRecorder::vert_to_track_recorder(std::usize index) -> TOBMuxRecorder& {
    if (index >= hardware::TOB::VERI_TO_TRACK_MUX_COUNT) {
        throw std::out_of_range("TOBRecorder::vert_to_track_recorder(): index out of range");
    }
    return this->_vert_to_track_recorder.at(index);
}

auto TOBRecorder::mux_chain_index(
    std::usize bump_index, std::usize track_index
) const -> std::Tuple<std::usize, std::usize, std::usize, std::usize, std::usize, std::usize> {
    auto bank = bump_index / (hardware::TOB::INDEX_SIZE/2);

    auto vert_mux = track_index % (hardware::TOB::INDEX_SIZE/2);          // [0, 63]
    auto vert_mux_output = track_index / (hardware::TOB::INDEX_SIZE/2);
    auto vert_mux_input = bank;
    auto vert_index = bank * 64 + vert_mux;
    
    auto hori_mux_input = vert_mux / hardware::TOB::HORI_TO_VERI_MUX_SIZE;
    auto hori_mux = bump_index / hardware::TOB::BUMP_TO_HORI_MUX_SIZE;
    auto hori_mux_output = vert_mux % hardware::TOB::VERI_TO_TRACK_MUX_SIZE;
    auto hori_index = 8*hori_mux + hori_mux_input;
    
    auto bump_mux = bump_index / hardware::TOB::BUMP_TO_HORI_MUX_SIZE;
    auto bump_mux_input = bump_index % hardware::TOB::BUMP_TO_HORI_MUX_SIZE;
    auto bump_mux_output = hori_mux_input;

    return std::Tuple<std::usize, std::usize, std::usize, std::usize, std::usize, std::usize> {
        bump_mux, bump_mux_input, hori_mux, hori_mux_input, vert_mux, vert_mux_input
    };
}

auto TOBRecorder::tob_cost(std::usize bump_index, std::usize track_index, bool reuse_type) const -> float {
    auto [bump_mux, bump_mux_input, hori_mux, hori_mux_input, vert_mux, vert_mux_input] = this->mux_chain_index(bump_index, track_index);
    float cost {0};
    auto calculate = [&](const TOBMuxRecorder& mux_recorder, std::usize index) -> float {
        auto [reuse_num, nonre_num] = mux_recorder.group_info();
        return mux_recorder.mux_cost(index, reuse_type);
    };
    cost += calculate(this->_bump_to_hori_recorder.at(bump_mux), bump_mux_input);
    cost += calculate(this->_hori_to_vert_recorder.at(hori_mux), hori_mux_input);
    cost += calculate(this->_vert_to_track_recorder.at(vert_mux), vert_mux_input);
    
    return cost;
}

auto TOBRecorder::update_type(std::usize bump_index, std::usize hori_index, std::usize vert_index, bool reuse_type) -> void {
    auto [bump_group, bump_group_index] = this->bump_group_info(bump_index);
    this->_bump_to_hori_recorder.at(bump_group).update_type(bump_group_index, reuse_type);

    auto [hori_group, hori_group_index] = this->hori_group_info(hori_index);
    this->_hori_to_vert_recorder.at(hori_group).update_type(hori_group_index, reuse_type);

    auto [vert_group, vert_group_index] = this->vert_group_info(vert_index);
    this->_vert_to_track_recorder.at(vert_group).update_type(vert_group_index, reuse_type);
}

auto TOBRecorder::update_history(std::usize bump_index, std::usize hori_index, std::usize vert_index, bool reuse_type) -> void {
    auto [bump_group, bump_group_index] = this->bump_group_info(bump_index);
    this->_bump_to_hori_recorder.at(bump_group).update_history(bump_group_index, reuse_type);
    
    auto [hori_group, hori_group_index] = this->hori_group_info(hori_index);
    this->_hori_to_vert_recorder.at(hori_group).update_history(hori_group_index, reuse_type);
    
    auto [vert_group, vert_group_index] = this->vert_group_info(vert_index);
    this->_vert_to_track_recorder.at(vert_group).update_history(vert_group_index, reuse_type);
}

auto TOBRecorder::update_cost(std::usize bump_index, std::usize hori_index, std::usize vert_index, bool reuse_type) -> void {
    auto [bump_group, bump_group_index] = this->bump_group_info(bump_index);
    this->_bump_to_hori_recorder.at(bump_group).update_cost();
    
    auto [hori_group, hori_group_index] = this->hori_group_info(hori_index);
    this->_hori_to_vert_recorder.at(hori_group).update_cost();

    auto [vert_group, vert_group_index] = this->vert_group_info(vert_index);
    this->_vert_to_track_recorder.at(vert_group).update_cost();
}

auto TOBRecorder::bump_group_info(std::usize bump_index) const -> std::tuple<std::usize, std::usize> {
    return {
        bump_index / hardware::TOB::BUMP_TO_HORI_MUX_SIZE,
        bump_index % hardware::TOB::BUMP_TO_HORI_MUX_SIZE,  
    };
}

auto TOBRecorder::hori_group_info(std::usize hori_index) const -> std::Tuple<std::usize, std::usize> {
    if (hori_index >= (hardware::TOB::INDEX_SIZE / 2)) {
        return {
            hori_index % hardware::TOB::HORI_TO_VERI_MUX_SIZE + (hardware::TOB::HORI_TO_VERI_MUX_COUNT / 2),
            (hori_index - hardware::TOB::INDEX_SIZE / 2) / hardware::TOB::HORI_TO_VERI_MUX_SIZE
        };
    } else {
        return {
            hori_index % hardware::TOB::HORI_TO_VERI_MUX_SIZE,
            hori_index / hardware::TOB::HORI_TO_VERI_MUX_SIZE, 
        };
    }
}

auto TOBRecorder::vert_group_info(std::usize vert_index) const -> std::Tuple<std::usize, std::usize> {
    if (vert_index >= (hardware::TOB::INDEX_SIZE / 2)) {
        return {
            vert_index - (hardware::TOB::INDEX_SIZE / 2),
            1
        };
    } else {
        return {
            vert_index,
            0
        };
    }
}

auto TOBRecorder::re_initialize() -> void {
    for (auto &recorder: this->_bump_to_hori_recorder) {
        recorder.re_initialize();
    }
    for (auto &recorder: this->_hori_to_vert_recorder) {
        recorder.re_initialize();
    }
    for (auto &recorder: this->_vert_to_track_recorder) {
        recorder.re_initialize();
    }
}

auto TOBRecorder::clear_history_record(std::usize bump_index, std::usize hori_index, std::usize vert_index) -> void {
    auto [bump_group, bump_group_index] = this->bump_group_info(bump_index);
    this->_bump_to_hori_recorder.at(bump_group).clear_history_record(bump_group_index);

    auto [hori_group, hori_group_index] = this->hori_group_info(hori_index);
    this->_hori_to_vert_recorder.at(hori_group).clear_history_record(hori_group_index);

    auto [vert_group, vert_group_index] = this->vert_group_info(vert_index);
    this->_vert_to_track_recorder.at(vert_group).clear_history_record(vert_group_index);
}

}


