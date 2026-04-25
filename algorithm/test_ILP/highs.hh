#pragma once

#include "ilp_types.hh"
#include "highs/lp_data/HConst.h"

#include <std/collection.hh>
#include <std/string.hh>

namespace PR_tool {

struct TobIlpNetAssignment {
    std::String net_name;
    std::size_t cob_unit;
};

struct TobIlpWAssignment {
    Bump_coord bump;
    std::size_t j;
    std::size_t k;
    std::size_t track;
    bool has_track;
    bool use_straight;
};

struct TobIlpSAssignment {
    std::size_t tob;
    std::size_t v;
    std::size_t j;
    std::size_t k;
};

struct TobIlpNetRouteDetail {
    std::String net_name;
    Bump_coord bump;
    std::size_t j;
    std::size_t k;
    std::size_t s_v;
    std::size_t track;
    std::size_t cob_unit;
    bool use_straight;
};

struct TobIlpResult {
    bool ok{false};
    std::String message;
    double objective{0.0};
    HighsModelStatus model_status{HighsModelStatus::kNotset};
    std::Vector<TobIlpNetAssignment> assignments;
    std::Vector<TobIlpWAssignment> active_w;
    std::Vector<TobIlpSAssignment> active_s;
    std::Vector<TobIlpNetRouteDetail> route_details;
};

auto solve_tob_ilp_with_highs(
    const std::Vector<Net_cost_record>& records,
    const std::Vector<Net_cost_matrix>& costs,
    bool enable_objective = false,
    bool enable_parallel = false
)
    -> TobIlpResult;

} // namespace PR_tool
