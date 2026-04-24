#include "highs.hh"
#include "tob_ilp_model.hh"

#include "highs/Highs.h"

#include <cmath>
#include <format>
#include <limits>

namespace PR_tool {

auto solve_tob_ilp_with_highs(const std::Vector<Net_cost_record>& records, const std::Vector<Net_cost_matrix>& costs)
    -> TobIlpResult {
    TobIlpResult out {};
    TobIlpModel model {};
    build_tob_ilp_model(model, records, costs);

    HighsLp lp {};
    std::Vector<std::array<HighsInt, 16>> z_col_index {};
    model.to_highs_lp(lp, z_col_index);

    Highs highs {};
    highs.setOptionValue("output_flag", false);
    const HighsStatus pass_st = highs.passModel(std::move(lp));
    if (pass_st != HighsStatus::kOk) {
        out.ok = false;
        out.message = std::format("HiGHS passModel failed (status={})", static_cast<int>(pass_st));
        return out;
    }

    const HighsStatus run_st = highs.run();
    if (run_st != HighsStatus::kOk) {
        out.ok = false;
        out.message = std::format("HiGHS run failed (status={})", static_cast<int>(run_st));
        out.model_status = highs.getModelStatus();
        return out;
    }

    out.model_status = highs.getModelStatus();
    out.objective = highs.getObjectiveValue();

    const auto& sol = highs.getSolution();
    if (sol.col_value.empty()) {
        out.ok = false;
        out.message = "HiGHS returned empty solution";
        return out;
    }

    constexpr double z_tol = 0.5;
    out.assignments.reserve(records.size());
    for (std::size_t n = 0; n < records.size(); ++n) {
        std::size_t chosen = std::numeric_limits<std::size_t>::max();
        int count = 0;
        for (std::size_t c = 0; c < 16; ++c) {
            const HighsInt zi = z_col_index[n][c];
            if (zi < 0) {
                continue;
            }
            if (static_cast<std::size_t>(zi) >= sol.col_value.size()) {
                out.ok = false;
                out.message = std::format("z column index out of range for net {}", n);
                return out;
            }
            const double v = sol.col_value[static_cast<std::size_t>(zi)];
            if (v > z_tol) {
                chosen = c;
                ++count;
            }
        }
        if (count != 1) {
            out.ok = false;
            out.message = std::format(
                "expected exactly one active Z for net index {} ('{}'), got {}",
                n,
                records[n].net_name,
                count
            );
            return out;
        }
        out.assignments.push_back(TobIlpNetAssignment {records[n].net_name, chosen});
    }

    out.ok = (out.model_status == HighsModelStatus::kOptimal);
    if (!out.ok && out.message.empty()) {
        out.message = std::format("model status is not optimal ({})", static_cast<int>(out.model_status));
    }
    else if (out.ok) {
        out.message = "ok";
    }
    return out;
}

} // namespace PR_tool
