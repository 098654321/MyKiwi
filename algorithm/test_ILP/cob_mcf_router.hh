#pragma once

#include "ilp_types.hh"

#include <hardware/interposer.hh>
#include <std/collection.hh>
#include <std/string.hh>

namespace PR_tool::circuit {
class BaseDie;
} // namespace PR_tool::circuit

namespace PR_tool {

struct TobIlpNetAssignment;

struct CobMcfCobUnitSummary {
    std::size_t cob_unit{0};
    int num_commodities{0};
    bool ok{false};
    double objective{0.0};
    int solve_ms{0};
    std::String message;
};

struct CobMcfRunSummary {
    std::Vector<CobMcfCobUnitSummary> per_cob;
    bool all_ok{true};
};

/// Per-cobunit MCF: getNetinCOBUnit → merge → build_commodities → build LP + HiGHS.
/// When \p enable_mcf_parallel is true, each unit is solved concurrently (separate HiGHS instances).
auto run_mcf_global_routing_cob_units(
    const std::Vector<Net_cost_record>& records,
    const std::Vector<TobIlpNetAssignment>& ilp_assignments,
    const hardware::Interposer& interposer,
    const circuit::BaseDie& basedie,
    bool enable_mcf_parallel = false
) -> CobMcfRunSummary;

} // namespace PR_tool
