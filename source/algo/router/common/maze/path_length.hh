#pragma once

#include <std/collection.hh>
#include <std/integer.hh>

#include <hardware/interposer.hh>



namespace PR_tool::hardware {
    class COBConnector;
}

namespace PR_tool::algo {

    static constexpr bool USE_SWITCH_LENGTH = false;
    using routed_path = std::Vector<std::Tuple<PR_tool::hardware::Track *, std::Option<PR_tool::hardware::COBConnector>>>;

    auto path_to_string(const std::Vector<hardware::Track*>& path) -> std::String;
    auto path_to_string(const routed_path& path) -> std::String;
    auto path_length(const routed_path& path, bool switch_length = USE_SWITCH_LENGTH) -> std::usize;
    auto path_length(const std::Vector<hardware::Track*>& path, bool switch_length = USE_SWITCH_LENGTH) -> std::usize;

    auto track_pos_to_cobs(const hardware::Track* track) -> std::Vector<hardware::COBCoord>;
    auto shared_cobs(const std::Vector<hardware::COBCoord>& cobs1, const std::Vector<hardware::COBCoord>& cobs2) -> std::Vector<hardware::COBCoord>;

}