#pragma once

#include "./recorder.hh"
#include <hardware/interposer.hh>
#include <ranges>


namespace kiwi::algo {
    
class TOBMuxRecorder {
public:
    TOBMuxRecorder(std::usize size);
    ~TOBMuxRecorder() = default;

public:
    auto size() const -> std::usize {return this->_size;}
    auto type_recorder(std::usize index) -> TypeRecorder&;
    auto shared_recorder(std::usize index) -> SharedRecorder&;

    // return <reuse_num, nonre_num>
    auto group_info() const -> std::Pair<std::usize, std::usize>;
    
    auto mux_cost(std::usize index, std::usize reuse_num, std::usize nonre_num) const -> float;

private:
    std::usize _size;
    std::Vector<TypeRecorder> _mux_type_recorder;
    std::Vector<SharedRecorder> _mux_shared_recorder;
};


class TOBRecorder {
public:
    TOBRecorder();
    ~TOBRecorder() = default;

public:
    auto bump_to_hori_recorder(std::usize index) -> TOBMuxRecorder&;
    auto hori_to_vert_recorder(std::usize index) -> TOBMuxRecorder&;
    auto vert_to_track_recorder(std::usize index) -> TOBMuxRecorder&;

    auto mux_chain_index(std::usize bump_index, std::usize track_index) const -> std::Tuple<std::usize, std::usize, std::usize, std::usize, std::usize, std::usize>;
    auto tob_cost(std::usize bump_index, std::usize track_index, bool reuse_type) const -> float;

private:
    std::Vector<TOBMuxRecorder> _bump_to_hori_recorder;
    std::Vector<TOBMuxRecorder> _hori_to_vert_recorder;
    std::Vector<TOBMuxRecorder> _vert_to_track_recorder;
};

}

