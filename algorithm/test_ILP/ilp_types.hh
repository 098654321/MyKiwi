#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <std/collection.hh>
#include <std/string.hh>

namespace PR_tool {

struct Bump_coord {
    std::size_t TOB;
    std::size_t Bank;
    std::size_t Group;
    std::size_t Index;

    auto operator==(const Bump_coord& other) const -> bool {
        return TOB == other.TOB && Bank == other.Bank && Group == other.Group && Index == other.Index;
    }

    auto operator<(const Bump_coord& other) const -> bool {
        return std::tie(TOB, Bank, Group, Index) < std::tie(other.TOB, other.Bank, other.Group, other.Index);
    }
};

enum class Net_type {
    Bnet,
    Tnet,
    PNnet
};

struct Net_cost_record {
    std::String net_name;
    Net_type type;
    float bits;
    float lambda;
    std::Vector<Bump_coord> start_bumps;
    std::Vector<Bump_coord> end_bumps;
    std::Vector<std::size_t> candidate_cobunits;
    std::Vector<std::size_t> tnet_fixed_cobunits;
};

using Bump_cost_row = std::array<double, 16>;
using Net_cost_matrix = std::map<Bump_coord, Bump_cost_row>;

inline auto map_track(std::size_t track) -> std::size_t {
    return track < 64 ? track % 8 : track % 8 + 8;
}

} // namespace PR_tool
