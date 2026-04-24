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

struct TobIlpResult {
    bool ok{false};
    std::String message;
    double objective{0.0};
    HighsModelStatus model_status{HighsModelStatus::kNotset};
    std::Vector<TobIlpNetAssignment> assignments;
};

auto solve_tob_ilp_with_highs(const std::Vector<Net_cost_record>& records, const std::Vector<Net_cost_matrix>& costs)
    -> TobIlpResult;

} // namespace PR_tool
