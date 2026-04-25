#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <std/collection.hh>
#include <std/string.hh>

#include <hardware/track/trackcoord.hh>

namespace PR_tool {

enum class IlpPowerKind {
    None,
    Pose,
    Nege
};

enum class IlpEndpointKind {
    Bump,
    Track
};

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

    /// Key to merge 2-pin fragments back to a logical net (prefix before first `__` in split names).
    std::String origin_key {};
    IlpPowerKind power_kind{IlpPowerKind::None};
    IlpEndpointKind mcf_start_kind{IlpEndpointKind::Bump};
    IlpEndpointKind mcf_end_kind{IlpEndpointKind::Bump};
    hardware::TrackCoord mcf_start_track {};
    hardware::TrackCoord mcf_end_track {};
    bool mcf_has_start_track{false};
    bool mcf_has_end_track{false};
};

using Bump_cost_row = std::array<double, 16>;
using Net_cost_matrix = std::map<Bump_coord, Bump_cost_row>;

inline auto map_track(std::size_t track) -> std::size_t {
    return track < 64 ? track % 8 : track % 8 + 8;
}

} // namespace PR_tool
