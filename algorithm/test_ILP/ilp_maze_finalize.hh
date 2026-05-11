#pragma once

#include "cob_mcf_router.hh"
#include "highs.hh"
#include "ilp_types.hh"

#include <circuit/basedie.hh>
#include <hardware/interposer.hh>
#include <std/string.hh>

namespace PR_tool {

auto run_ilp_maze_finalize(
    const std::Vector<Net_cost_record>& records,
    const TobIlpResult& ilp_result,
    const CobMcfFullResult& mcf_result,
    hardware::Interposer* interposer,
    const circuit::BaseDie* basedie,
    CobMcfGridDims cob_grid
) -> bool;

} // namespace PR_tool
