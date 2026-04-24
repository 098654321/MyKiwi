#include "highs.hh"
#include "tob_ilp_model.hh"

#include "highs/Highs.h"

#include <algorithm>
#include <format>
#include <limits>
#include <map>
#include <set>
#include <thread>

namespace PR_tool {

auto solve_tob_ilp_with_highs(const std::Vector<Net_cost_record>& records, const std::Vector<Net_cost_matrix>& costs)
    -> TobIlpResult {
    TobIlpResult out {};
    TobIlpModel model {};
    build_tob_ilp_model(model, records, costs);     // 建模ILP

    HighsLp lp {};
    std::Vector<std::array<HighsInt, 16>> z_col_index {};
    std::map<std::String, HighsInt> col_index {};
    model.to_highs_lp(lp, z_col_index, &col_index); // 转换为HiGHS LP

    Highs highs {};
    highs.setOptionValue("output_flag", false);
    const unsigned int hw_threads = std::thread::hardware_concurrency();
    const HighsInt threads = static_cast<HighsInt>(hw_threads > 1U ? hw_threads : 1U);
    const HighsStatus parallel_st = highs.setOptionValue("parallel", "on");
    if (parallel_st != HighsStatus::kOk) {
        (void)highs.setOptionValue("parallel", "choose");
    }
    const HighsStatus thread_st = highs.setOptionValue("threads", threads);
    if (thread_st != HighsStatus::kOk) {
        (void)highs.setOptionValue("threads", static_cast<HighsInt>(1));
    }
    const HighsStatus pass_st = highs.passModel(std::move(lp));  // 传递LP给HiGHS
    if (pass_st != HighsStatus::kOk) {
        out.ok = false;
        out.message = std::format("HiGHS passModel failed (status={})", static_cast<int>(pass_st));
        return out;
    }

    const HighsStatus run_st = highs.run();    // 运行HiGHS
    if (run_st != HighsStatus::kOk) {
        out.ok = false;
        out.message = std::format("HiGHS run failed (status={})", static_cast<int>(run_st));
        out.model_status = highs.getModelStatus();
        return out;
    }

    out.model_status = highs.getModelStatus();    // 获取模型状态
    out.objective = highs.getObjectiveValue();    // 获取目标值

    const auto& sol = highs.getSolution();
    if (sol.col_value.empty()) {
        out.ok = false;
        out.message = "HiGHS returned empty solution";
        return out;
    }

    constexpr double z_tol = 0.5;
    const auto is_active = [&](const std::String& var_name) -> bool {
        const auto it = col_index.find(var_name);
        if (it == col_index.end() || it->second < 0) {
            return false;
        }
        const auto idx = static_cast<std::size_t>(it->second);
        if (idx >= sol.col_value.size()) {
            out.ok = false;
            out.message = std::format("column index out of range for variable '{}'", var_name);
            return false;
        }
        return sol.col_value[idx] > z_tol;
    };
    const auto track_from_jk = [](const std::size_t bank, const std::size_t j, const std::size_t k, const bool straight) -> std::size_t {
        const auto v = j * 8 + k;
        std::size_t track = 0;
        if (bank == 0) {
            track = straight ? v : (v + 64);
        }
        else {
            track = straight ? (v + 64) : v;
        }
        return track;
    };
    const auto cob_from_jk = [&](const std::size_t bank, const std::size_t j, const std::size_t k, const bool straight) -> std::size_t {
        return map_track(track_from_jk(bank, j, k, straight));
    };
    const auto relation_bumps_for = [](const Net_cost_record& record) {
        auto relation_bumps = std::Vector<Bump_coord> {};
        if (record.type == Net_type::Bnet) {
            relation_bumps.insert(relation_bumps.end(), record.start_bumps.begin(), record.start_bumps.end());
            relation_bumps.insert(relation_bumps.end(), record.end_bumps.begin(), record.end_bumps.end());
        }
        else {
            relation_bumps.insert(relation_bumps.end(), record.start_bumps.begin(), record.start_bumps.end());
        }
        std::sort(relation_bumps.begin(), relation_bumps.end());
        relation_bumps.erase(std::unique(relation_bumps.begin(), relation_bumps.end()), relation_bumps.end());
        return relation_bumps;
    };

    out.assignments.reserve(records.size());
    for (std::size_t n = 0; n < records.size(); ++n) {
        std::size_t chosen = std::numeric_limits<std::size_t>::max();
        int count = 0;
        for (std::size_t c = 0; c < 16; ++c) {
            // 获取z列索引
            const HighsInt zi = z_col_index[n][c];
            if (zi < 0) {
                continue;
            }
            // 检查z列索引是否超出范围
            if (static_cast<std::size_t>(zi) >= sol.col_value.size()) {
                out.ok = false;
                out.message = std::format("z column index out of range for net {}", n);
                return out;
            }
            const double v = sol.col_value[static_cast<std::size_t>(zi)];    // 获取z列的值
            if (v > z_tol) {    // 判断是否激活
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

    auto all_related_bumps = std::set<Bump_coord> {};
    for (const auto& record : records) {
        const auto relation_bumps = relation_bumps_for(record);
        all_related_bumps.insert(relation_bumps.begin(), relation_bumps.end());
    }

    auto active_s_by_tv = std::set<std::pair<std::size_t, std::size_t>> {};
    out.active_s.reserve(all_related_bumps.size() * 8);
    for (const auto& bump : all_related_bumps) {
        for (std::size_t v = 0; v < 64; ++v) {
            if (!is_active(s_var(bump.TOB, v))) {
                continue;
            }
            if (!active_s_by_tv.insert({bump.TOB, v}).second) {
                continue;
            }
            out.active_s.push_back(TobIlpSAssignment {bump.TOB, v, v / 8, v % 8});
        }
    }

    for (const auto& bump : all_related_bumps) {
        for (std::size_t j = 0; j < 8; ++j) {
            for (std::size_t k = 0; k < 8; ++k) {
                if (!is_active(w_var(bump, j, k))) {
                    continue;
                }
                const bool qs_active = is_active(qs_var(bump, j, k));
                const bool qw_active = is_active(qw_var(bump, j, k));
                const bool has_track = (qs_active != qw_active);
                const bool use_straight = qs_active && !qw_active;
                const std::size_t track = has_track ? track_from_jk(bump.Bank, j, k, use_straight)
                                                    : std::numeric_limits<std::size_t>::max();
                out.active_w.push_back(TobIlpWAssignment {bump, j, k, track, has_track, use_straight});
            }
        }
    }

    out.route_details.reserve(records.size() * 4);
    for (std::size_t n = 0; n < records.size(); ++n) {
        const auto relation_bumps = relation_bumps_for(records[n]);
        for (const auto& bump : relation_bumps) {
            for (std::size_t j = 0; j < 8; ++j) {
                for (std::size_t k = 0; k < 8; ++k) {
                    const bool qs_active = is_active(qs_var(bump, j, k));
                    const bool qw_active = is_active(qw_var(bump, j, k));
                    if (qs_active == qw_active) {
                        continue;
                    }
                    const bool use_straight = qs_active;
                    const std::size_t s_v = j * 8 + k;
                    out.route_details.push_back(TobIlpNetRouteDetail {
                        records[n].net_name,
                        bump,
                        j,
                        k,
                        s_v,
                        track_from_jk(bump.Bank, j, k, use_straight),
                        cob_from_jk(bump.Bank, j, k, use_straight),
                        use_straight
                    });
                }
            }
        }
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
