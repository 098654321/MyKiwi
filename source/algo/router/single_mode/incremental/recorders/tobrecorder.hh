#pragma once

#include "./recorder.hh"
#include <hardware/interposer.hh>
#include <ranges>


namespace kiwi::algo {
    
class TOBMuxRecorder {
public:
    TOBMuxRecorder(std::usize size, bool use_cost);
    ~TOBMuxRecorder() = default;

public:
    auto size() const -> std::usize {return this->_size;}
    auto type_recorder(std::usize index) -> TypeRecorder&;

    // return <reuse_num, nonre_num>
    auto group_info() const -> std::Pair<float, float>;
    
    auto mux_cost(std::usize index, bool reuse_type) const -> float;
    auto update_type(std::usize index, bool reuse_type) -> void;
    auto update_history(std::usize index, bool reuse_type) -> void;
    auto update_cost() -> void;
    auto re_initialize() -> void;
    auto clear_history_record(std::usize index) -> void;
    auto show_data(std::tuple<std::size_t, std::size_t> mux_info, bool show_all, bool print = false) const -> std::string;
    // return: <cost_reuse, cost_nonreuse>
    auto self_cost() const -> std::tuple<float, float>;

private:
    std::usize _size;
    std::Vector<TypeRecorder> _mux_type_recorder;
};


class TOBRecorder {
public:
    TOBRecorder(bool use_cost);
    ~TOBRecorder() = default;

public:
    auto bump_to_hori_recorder(std::usize index) -> TOBMuxRecorder&;
    auto bump_to_hori_recorder(std::usize index) const -> const TOBMuxRecorder&;
    auto hori_to_vert_recorder(std::usize index) -> TOBMuxRecorder&;
    auto hori_to_vert_recorder(std::usize index) const -> const TOBMuxRecorder&;
    auto vert_to_track_recorder(std::usize index) -> TOBMuxRecorder&;
    auto vert_to_track_recorder(std::usize index) const -> const TOBMuxRecorder&;

    auto mux_chain_index(std::usize bump_index, std::usize track_index) const -> std::Tuple<std::usize, std::usize, std::usize, std::usize, std::usize, std::usize>;
    auto tob_cost(std::usize bump_index, std::usize track_index, bool reuse_type) const -> float;

    auto update_type(std::usize bump_index, std::usize hori_index, std::usize vert_index, bool reuse_type) -> void;
    auto update_history(std::usize bump_index, std::usize hori_index, std::usize vert_index, bool reuse_type) -> void;
    auto update_cost(std::usize bump_index, std::usize hori_index, std::usize vert_index, bool reuse_type) -> void;
    auto re_initialize() -> void;
    auto clear_history_record(std::usize bump_index, std::usize hori_index, std::usize vert_index) -> void;
    auto show_data(std::size_t bump_index, std::size_t hori_index, std::size_t vert_index, bool show_all, bool print = false) const -> std::string;
    // return: <cost_reuse, cost_nonreuse>
    auto self_cost() const -> std::tuple<float, float>;

private:
    auto bump_group_info(std::usize bump_index) const -> std::tuple<std::usize, std::usize>;
    auto hori_group_info(std::usize hori_index) const -> std::Tuple<std::usize, std::usize>;
    auto vert_group_info(std::usize vert_index) const -> std::Tuple<std::usize, std::usize>;

private:
    std::Vector<TOBMuxRecorder> _bump_to_hori_recorder;
    std::Vector<TOBMuxRecorder> _hori_to_vert_recorder;
    std::Vector<TOBMuxRecorder> _vert_to_track_recorder;
};

}

