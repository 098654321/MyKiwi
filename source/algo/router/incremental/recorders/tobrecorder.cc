#include "./tobrecorder.hh"
#include <debug/debug.hh>
#include <stdexcept>
#include <format>


namespace kiwi::algo {

TOBMuxRecorder::TOBMuxRecorder(std::usize size) : _size(size) {
    this->_mux_type_recorder.reserve(size);
    for (auto i: std::views::iota(0, (int)size)) {
        this->_mux_type_recorder.emplace_back(TypeRecorder{});
    }

    this->_mux_shared_recorder.reserve(size);
    for (auto i: std::views::iota(0, (int)size)) {
        this->_mux_shared_recorder.emplace_back(SharedRecorder{});
    }
}

auto TOBMuxRecorder::type_recorder(std::usize index) -> TypeRecorder& {
    if (index >= this->_size) {
        throw std::out_of_range("TOBMuxRecorder::type_recorder(): index out of range");
    }
    return this->_mux_type_recorder[index];
}

auto TOBMuxRecorder::shared_recorder(std::usize index) -> SharedRecorder& {
    if (index >= this->_size) {
        throw std::out_of_range("TOBMuxRecorder::shared_recorder(): index out of range");
    }
    return this->_mux_shared_recorder[index];
}

auto TOBMuxRecorder::mux_cost(std::usize index, std::usize reuse_num, std::usize nonre_num) const -> float {
    if (index >= this->_size) {
        throw std::out_of_range(std::format("index {} is out of range [0, {})", index, this->_size));
    }
    return this->_mux_shared_recorder[index].cost() + this->_mux_type_recorder[index].cost(reuse_num, nonre_num);
}

auto TOBMuxRecorder::group_info() const -> std::Pair<std::usize, std::usize> {
    std::usize reuse_num = 0, nonre_num = 0;
    for (auto& recorder: this->_mux_type_recorder) {
        if (recorder.current_type().has_value()) {
            recorder.current_type().value() ? reuse_num++ : nonre_num++;
        } 
    }
    return std::Pair<std::usize, std::usize>{reuse_num, nonre_num};
}

TOBRecorder::TOBRecorder() {
    this->_bump_to_hori_recorder.reserve(hardware::TOB::BUMP_TO_HORI_MUX_COUNT);
    for (auto i: std::views::iota(0, (int)hardware::TOB::BUMP_TO_HORI_MUX_COUNT)) {
        this->_bump_to_hori_recorder.emplace_back(TOBMuxRecorder{hardware::TOB::BUMP_TO_HORI_MUX_SIZE});
    }

    this->_hori_to_vert_recorder.reserve(hardware::TOB::HORI_TO_VERI_MUX_COUNT);
    for (auto i: std::views::iota(0, (int)hardware::TOB::HORI_TO_VERI_MUX_COUNT)) {
        this->_hori_to_vert_recorder.emplace_back(TOBMuxRecorder{hardware::TOB::HORI_TO_VERI_MUX_SIZE});
    }

    this->_vert_to_track_recorder.reserve(hardware::TOB::VERI_TO_TRACK_MUX_COUNT);
    for (auto i: std::views::iota(0, (int)hardware::TOB::VERI_TO_TRACK_MUX_COUNT)) {
        this->_vert_to_track_recorder.emplace_back(TOBMuxRecorder{hardware::TOB::VERI_TO_TRACK_MUX_SIZE});
    }
}

auto TOBRecorder::bump_to_hori_recorder(std::usize index) -> TOBMuxRecorder& {
    if (index >= hardware::TOB::BUMP_TO_HORI_MUX_COUNT) {
        throw std::out_of_range("TOBRecorder::bump_to_hori_recorder(): index out of range");
    }
    return this->_bump_to_hori_recorder[index];
}

auto TOBRecorder::hori_to_vert_recorder(std::usize index) -> TOBMuxRecorder& {
    if (index >= hardware::TOB::HORI_TO_VERI_MUX_COUNT) {
        throw std::out_of_range("TOBRecorder::hori_to_vert_recorder(): index out of range");
    }
    return this->_hori_to_vert_recorder[index];
}

auto TOBRecorder::vert_to_track_recorder(std::usize index) -> TOBMuxRecorder& {
    if (index >= hardware::TOB::VERI_TO_TRACK_MUX_COUNT) {
        throw std::out_of_range("TOBRecorder::vert_to_track_recorder(): index out of range");
    }
    return this->_vert_to_track_recorder[index];
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
    std::usize cost {0};
    auto calculate = [&](const TOBMuxRecorder& mux_recorder, std::usize index) -> float {
        auto [reuse_num, nonre_num] = mux_recorder.group_info();
        return mux_recorder.mux_cost(index, reuse_num, nonre_num);
    };
    cost += calculate(this->_bump_to_hori_recorder[bump_mux], bump_mux_input);
    cost += calculate(this->_hori_to_vert_recorder[hori_mux], hori_mux_input);
    cost += calculate(this->_vert_to_track_recorder[vert_mux], vert_mux_input);
    
    return cost;
}




}


