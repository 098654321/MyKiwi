#include "tob_ilp_model.hh"
#include "ilp_speedup.hh"

#include "highs/lp_data/HConst.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <std/utility.hh>

namespace PR_tool {

auto w_var(const Bump_coord& b, std::size_t j, std::size_t k) -> std::String {
    return std::format("W_{}_{}_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index, j, k);
}

auto s_var(const std::size_t t, const std::size_t v) -> std::String {
    return std::format("S_{}_{}", t, v);
}

auto z_var(const std::size_t n, const std::size_t c) -> std::String {
    return std::format("Z_{}_{}", n, c);
}

auto qs_var(const Bump_coord& b, const std::size_t j, const std::size_t k) -> std::String {
    return std::format("QS_{}_{}_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index, j, k);
}

auto qw_var(const Bump_coord& b, const std::size_t j, const std::size_t k) -> std::String {
    return std::format("QW_{}_{}_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index, j, k);
}

auto TobIlpModel::add_row(std::String name, const char type, const double rhs) -> void {
    if (this->_row_types.contains(name)) {
        return;
    }
    this->_row_types.emplace(name, type);
    this->_row_order.emplace_back(name);
    if (type != 'N') {
        this->_rhs.emplace(name, rhs);
    }
}

auto TobIlpModel::add_binary(std::String var_name) -> void {
    this->_binary_vars.insert(std::move(var_name));
}

auto TobIlpModel::add_objective(std::String var_name, const double coeff) -> void {
    if (std::fabs(coeff) < 1e-12) {
        return;
    }
    this->_objective[var_name] += coeff;
}

auto TobIlpModel::add_coefficient(std::String var_name, std::String row_name, const double coeff) -> void {
    if (std::fabs(coeff) < 1e-12) {
        return;
    }
    this->_columns[var_name].emplace_back(std::move(row_name), coeff);
}

auto TobIlpModel::write_mps(const std::String& path) const -> void {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error(std::format("cannot open mps output '{}'", path));
    }

    out << "NAME TOB_ALLOC\n";
    out << "ROWS\n";
    for (const auto& row_name : this->_row_order) {
        out << ' ' << this->_row_types.at(row_name) << ' ' << row_name << '\n';
    }

    out << "COLUMNS\n";
    for (const auto& [var_name, entries] : this->_columns) {
        if (const auto it = this->_objective.find(var_name); it != this->_objective.end()) {
            out << "    " << var_name << " OBJ " << it->second << '\n';
        }
        for (const auto& [row_name, coeff] : entries) {
            out << "    " << var_name << ' ' << row_name << ' ' << coeff << '\n';
        }
    }

    out << "RHS\n";
    for (const auto& row_name : this->_row_order) {
        if (const auto it = this->_rhs.find(row_name); it != this->_rhs.end()) {
            if (std::fabs(it->second) < 1e-12) {
                continue;
            }
            out << "    RHS1 " << row_name << ' ' << it->second << '\n';
        }
    }

    out << "BOUNDS\n";
    for (const auto& var_name : this->_binary_vars) {
        out << " BV BND1 " << var_name << '\n';
    }
    out << "ENDATA\n";
}

auto TobIlpModel::to_highs_lp(
    HighsLp& lp,
    std::Vector<std::array<HighsInt, 16>>& z_col_index,
    std::map<std::String, HighsInt>* col_index
) const -> void {
    lp.clear();

    std::map<std::String, HighsInt> row_to_idx;
    HighsInt row_idx = 0;
    for (const auto& row_name : this->_row_order) {
        const auto it_ty = this->_row_types.find(row_name);
        if (it_ty == this->_row_types.end() || it_ty->second == 'N') {
            continue;
        }
        row_to_idx.emplace(row_name, row_idx++);
    }
    const HighsInt num_row = row_idx;

    std::map<std::String, HighsInt> col_to_idx;
    std::Vector<std::String> col_order;
    col_order.reserve(this->_columns.size());
    {
        HighsInt col_idx = 0;
        for (const auto& [var_name, entries] : this->_columns) {
            (void)entries;
            col_to_idx.emplace(var_name, col_idx++);
            col_order.push_back(var_name);
        }
    }
    if (col_index != nullptr) {
        *col_index = col_to_idx;
    }
    const HighsInt num_col = static_cast<HighsInt>(col_order.size());

    z_col_index.clear();
    z_col_index.resize(this->_net_count);
    for (auto& row : z_col_index) {
        row.fill(static_cast<HighsInt>(-1));
    }
    for (std::size_t n = 0; n < this->_net_count; ++n) {
        for (std::size_t c = 0; c < 16; ++c) {
            const std::String zn = z_var(n, c);
            if (const auto it = col_to_idx.find(zn); it != col_to_idx.end()) {
                z_col_index[n][c] = it->second;
            }
        }
    }

    std::vector<HighsInt> a_start(static_cast<std::size_t>(num_col) + 1, 0);
    std::vector<HighsInt> a_index;
    std::vector<double> a_value;
    a_index.reserve((this->_columns.size() * 8u) + 1u);
    a_value.reserve((this->_columns.size() * 8u) + 1u);

    std::vector<double> col_cost(static_cast<std::size_t>(num_col), 0.0);
    for (HighsInt j = 0; j < num_col; ++j) {
        a_start[static_cast<std::size_t>(j)] = static_cast<HighsInt>(a_index.size());
        const std::String& vname = col_order[static_cast<std::size_t>(j)];
        if (const auto o = this->_objective.find(vname); o != this->_objective.end()) {
            col_cost[static_cast<std::size_t>(j)] = o->second;
        }
        for (const auto& [rname, coeff] : this->_columns.at(vname)) {
            const auto rit = row_to_idx.find(rname);
            if (rit == row_to_idx.end()) {
                throw std::runtime_error(std::format("tob ilp: row '{}' missing from index map (OBJ-only row?)", rname));
            }
            a_index.push_back(rit->second);
            a_value.push_back(coeff);
        }
    }
    a_start[static_cast<std::size_t>(num_col)] = static_cast<HighsInt>(a_index.size());

    std::vector<double> row_lower(static_cast<std::size_t>(num_row), -kHighsInf);
    std::vector<double> row_upper(static_cast<std::size_t>(num_row), kHighsInf);
    for (const auto& row_name : this->_row_order) {
        const auto tit = this->_row_types.find(row_name);
        if (tit == this->_row_types.end() || tit->second == 'N') {
            continue;
        }
        const auto rit = row_to_idx.find(row_name);
        if (rit == row_to_idx.end()) {
            continue;
        }
        const HighsInt r = rit->second;
        const char ty = tit->second;
        const double rhs = this->_rhs.contains(row_name) ? this->_rhs.at(row_name) : 0.0;
        if (ty == 'E') {
            row_lower[static_cast<std::size_t>(r)] = rhs;
            row_upper[static_cast<std::size_t>(r)] = rhs;
        }
        else if (ty == 'L') {
            row_lower[static_cast<std::size_t>(r)] = -kHighsInf;
            row_upper[static_cast<std::size_t>(r)] = rhs;
        }
        else if (ty == 'G') {
            row_lower[static_cast<std::size_t>(r)] = rhs;
            row_upper[static_cast<std::size_t>(r)] = kHighsInf;
        }
    }

    lp.num_col_ = num_col;
    lp.num_row_ = num_row;
    lp.sense_ = ObjSense::kMinimize;
    lp.offset_ = 0.0;
    lp.col_cost_ = std::move(col_cost);
    lp.col_lower_.assign(static_cast<std::size_t>(num_col), 0.0);
    lp.col_upper_.assign(static_cast<std::size_t>(num_col), 1.0);
    lp.row_lower_ = std::move(row_lower);
    lp.row_upper_ = std::move(row_upper);
    lp.integrality_.assign(static_cast<std::size_t>(num_col), HighsVarType::kInteger);
    lp.model_name_ = "TOB_ALLOC";
    lp.a_matrix_.format_ = MatrixFormat::kColwise;
    lp.a_matrix_.num_col_ = num_col;
    lp.a_matrix_.num_row_ = num_row;
    lp.a_matrix_.start_ = std::move(a_start);
    lp.a_matrix_.index_ = std::move(a_index);
    lp.a_matrix_.value_ = std::move(a_value);
    lp.setMatrixDimensions();
}

// MARK: build_tob_ilp_model (from former write_mps_file)
void build_tob_ilp_model(
    TobIlpModel& mps,
    const std::Vector<Net_cost_record>& records,
    const std::Vector<Net_cost_matrix>& costs,
    const bool enable_objective
) {
    if (records.size() != costs.size()) {
        throw std::runtime_error("records and costs size mismatch");
    }
    mps.set_net_count(records.size());

    mps.add_row("OBJ", 'N');

    auto active_bumps = std::set<Bump_coord> {};
    auto active_i = std::map<std::tuple<std::size_t, std::size_t, std::size_t>, std::set<std::size_t>> {};

    for (std::size_t n = 0; n < records.size(); ++n) {
        const auto& record = records[n];

        const auto z_sum_row = std::format("R_ZSUM_{}", n);
        mps.add_row(z_sum_row, 'E', 1.0);   // 约束4
        for (std::size_t c = 0; c < 16; ++c) {
            const auto z = z_var(n, c); // 创建变量z_{n, c}
            mps.add_binary(z);
            mps.add_coefficient(z, z_sum_row, 1.0);
            double coeff = 0.0;
            for (const auto& bump : record.start_bumps) {
                const auto it = costs[n].find(bump);
                if (it == costs[n].end()) {
                    throw std::runtime_error(std::format(
                        "missing bump cost at net '{}' bump({}, {}, {}, {})",
                        record.net_name, bump.TOB, bump.Bank, bump.Group, bump.Index
                    ));
                }
                coeff += it->second[c];
            }
            if (enable_objective) {
                mps.add_objective(z, coeff); // 可选目标函数
            }
        }

        // 约束5，分三种情况
        // Tnet：直接设置等式约束
        if (record.type == Net_type::Tnet) {
            for (const auto fixed_c : record.tnet_fixed_cobunits) {
                const auto row = std::format("R_TFIX_{}_{}", n, fixed_c);
                mps.add_row(row, 'E', 1.0);
                mps.add_coefficient(z_var(n, fixed_c), row, 1.0);
            }
            const auto allowed_jk = tnet_allowed_jk(record.tnet_fixed_cobunits);
            for (const auto& b : record.start_bumps) {
                const auto allow_row = std::format("R_TJK1_{}_{}_{}_{}_{}", n, b.TOB, b.Bank, b.Group, b.Index);
                mps.add_row(allow_row, 'E', 1.0);
                for (std::size_t j = 0; j < 8; ++j) {
                    for (std::size_t k = 0; k < 8; ++k) {
                        const auto w = w_var(b, j, k);
                        mps.add_binary(w);
                        if (allowed_jk.contains({j, k})) {
                            mps.add_coefficient(w, allow_row, 1.0);
                        }
                        else {
                            const auto zero_row = std::format(
                                "R_TJK0_{}_{}_{}_{}_{}_{}_{}",
                                n,
                                b.TOB,
                                b.Bank,
                                b.Group,
                                b.Index,
                                j,
                                k
                            );
                            mps.add_row(zero_row, 'E', 0.0);
                            mps.add_coefficient(w, zero_row, 1.0);
                        }
                    }
                }
            }
        }
        // PNnet：设置等式约束
        else if (record.type == Net_type::PNnet) {
            std::set<std::size_t> allowed(record.candidate_cobunits.begin(), record.candidate_cobunits.end());
            for (std::size_t c = 0; c < 16; ++c) {
                if (allowed.contains(c)) {
                    continue;
                }
                const auto row = std::format("R_PN0_{}_{}", n, c);
                mps.add_row(row, 'E', 0.0);
                mps.add_coefficient(z_var(n, c), row, 1.0);
            }
        }

        // Bnet：先存下来，后面处理
        for (const auto& b : record.start_bumps) {
            active_bumps.insert(b);
            active_i[{b.TOB, b.Bank, b.Group}].insert(b.Index);
        }
        for (const auto& b : record.end_bumps) {
            active_bumps.insert(b);
            active_i[{b.TOB, b.Bank, b.Group}].insert(b.Index);
        }
    }

    // 约束1
    for (const auto& b : active_bumps) {    // 约束6
        const auto row = std::format("R_WONE_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index);
        mps.add_row(row, 'E', 1.0);
        for (std::size_t j = 0; j < 8; ++j) {
            for (std::size_t k = 0; k < 8; ++k) {
                const auto w = w_var(b, j, k);
                mps.add_binary(w);
                mps.add_coefficient(w, row, 1.0);
            }
        }
    }

    // 约束2
    for (std::size_t t = 0; t < 16; ++t) {
        for (std::size_t b = 0; b < 2; ++b) {
            for (std::size_t g = 0; g < 8; ++g) {
                const auto key = std::tuple<std::size_t, std::size_t, std::size_t>{t, b, g};
                if (!active_i.contains(key)) {
                    continue;
                }
                for (std::size_t j = 0; j < 8; ++j) {
                    const auto row = std::format("R_HORI_{}_{}_{}_{}", t, b, g, j);
                    mps.add_row(row, 'L', 1.0);
                    for (const auto i : active_i.at(key)) {
                        for (std::size_t k = 0; k < 8; ++k) {
                            mps.add_coefficient(w_var(Bump_coord{t, b, g, i}, j, k), row, 1.0);
                        }
                    }
                }
            }
        }
    }

    // 约束3
    for (std::size_t t = 0; t < 16; ++t) {
        for (std::size_t b = 0; b < 2; ++b) {
            bool tb_active = false;
            for (std::size_t g = 0; g < 8; ++g) {
                if (active_i.contains(std::tuple<std::size_t, std::size_t, std::size_t>{t, b, g})) {
                    tb_active = true;
                    break;
                }
            }
            if (!tb_active) {
                continue;
            }
            for (std::size_t j = 0; j < 8; ++j) {
                for (std::size_t k = 0; k < 8; ++k) {
                    const auto row = std::format("R_VERT_{}_{}_{}_{}", t, b, j, k);
                    mps.add_row(row, 'L', 1.0);
                    for (std::size_t g = 0; g < 8; ++g) {
                        const auto gkey = std::tuple<std::size_t, std::size_t, std::size_t>{t, b, g};
                        if (!active_i.contains(gkey)) {
                            continue;
                        }
                        for (const auto i : active_i.at(gkey)) {
                            mps.add_coefficient(w_var(Bump_coord{t, b, g, i}, j, k), row, 1.0);
                        }
                    }
                }
            }
        }
    }

    auto ensure_q_linearization = [&](const Bump_coord& b, const std::size_t j, const std::size_t k) {
        const auto w = w_var(b, j, k);
        const auto qs = qs_var(b, j, k);
        const auto qw = qw_var(b, j, k);
        const auto s = s_var(b.TOB, j * 8 + k);

        mps.add_binary(w);
        mps.add_binary(s);
        mps.add_binary(qs);
        mps.add_binary(qw);

        {
            const auto row = std::format("R_QS1_{}_{}_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index, j, k);
            mps.add_row(row, 'L', 0.0);
            mps.add_coefficient(qs, row, 1.0);
            mps.add_coefficient(w, row, -1.0);
        }
        {
            const auto row = std::format("R_QS2_{}_{}_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index, j, k);
            mps.add_row(row, 'L', 0.0);
            mps.add_coefficient(qs, row, 1.0);
            mps.add_coefficient(s, row, -1.0);
        }
        {
            const auto row = std::format("R_QS3_{}_{}_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index, j, k);
            mps.add_row(row, 'G', -1.0);
            mps.add_coefficient(qs, row, 1.0);
            mps.add_coefficient(w, row, -1.0);
            mps.add_coefficient(s, row, -1.0);
        }

        {
            const auto row = std::format("R_QW1_{}_{}_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index, j, k);
            mps.add_row(row, 'L', 0.0);
            mps.add_coefficient(qw, row, 1.0);
            mps.add_coefficient(w, row, -1.0);
        }
        {
            const auto row = std::format("R_QW2_{}_{}_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index, j, k);
            mps.add_row(row, 'L', 1.0);
            mps.add_coefficient(qw, row, 1.0);
            mps.add_coefficient(s, row, 1.0);
        }
        {
            const auto row = std::format("R_QW3_{}_{}_{}_{}_{}_{}", b.TOB, b.Bank, b.Group, b.Index, j, k);
            mps.add_row(row, 'G', 0.0);
            mps.add_coefficient(qw, row, 1.0);
            mps.add_coefficient(w, row, -1.0);
            mps.add_coefficient(s, row, 1.0);
        }
    };

    const auto cob_from_jk = [](const std::size_t bank, const std::size_t j, const std::size_t k, const bool straight) -> std::size_t {
        const auto v = j * 8 + k;
        std::size_t track = 0;
        if (bank == 0) {
            track = straight ? v : (v + 64);
        }
        else {
            track = straight ? (v + 64) : v;
        }
        return map_track(track);
    };

    auto processed_q = std::set<std::tuple<Bump_coord, std::size_t, std::size_t>> {};

    const auto add_u_equals_z_rows = [&](const std::size_t net_idx, const Bump_coord& b, const std::String& row_label) {
        for (std::size_t c = 0; c < 16; ++c) {
            const auto row = std::format("R_{}_{}_{}_{}_{}_{}_{}", row_label, net_idx, b.TOB, b.Bank, b.Group, b.Index, c);
            mps.add_row(row, 'E', 0.0);
            mps.add_coefficient(z_var(net_idx, c), row, -1.0);
            for (std::size_t j = 0; j < 8; ++j) {
                for (std::size_t k = 0; k < 8; ++k) {
                    if (cob_from_jk(b.Bank, j, k, true) == c) {
                        mps.add_coefficient(qs_var(b, j, k), row, 1.0);
                    }
                    if (cob_from_jk(b.Bank, j, k, false) == c) {
                        mps.add_coefficient(qw_var(b, j, k), row, 1.0);
                    }
                }
            }
        }
    };

    const auto ensure_qs_qw = [&](const Bump_coord& b) {
        for (std::size_t j = 0; j < 8; ++j) {
            for (std::size_t k = 0; k < 8; ++k) {
                const auto key = std::tuple<Bump_coord, std::size_t, std::size_t>{b, j, k};
                if (!processed_q.contains(key)) {
                    ensure_q_linearization(b, j, k);
                    processed_q.insert(key);
                }
            }
        }
    };

    // Bnet 的约束5
    for (std::size_t n = 0; n < records.size(); ++n) {
        const auto& record = records[n];
        if (record.type != Net_type::Bnet) {
            continue;
        }
        auto relation_bumps = std::Vector<Bump_coord> {};
        relation_bumps.insert(relation_bumps.end(), record.start_bumps.begin(), record.start_bumps.end());
        relation_bumps.insert(relation_bumps.end(), record.end_bumps.begin(), record.end_bumps.end());
        std::sort(relation_bumps.begin(), relation_bumps.end());
        relation_bumps.erase(std::unique(relation_bumps.begin(), relation_bumps.end()), relation_bumps.end());

        for (const auto& b : relation_bumps) {
            ensure_qs_qw(b);
            add_u_equals_z_rows(n, b, "BEQ");
        }
    }

    for (std::size_t n = 0; n < records.size(); ++n) {
        const auto& record = records[n];
        if (record.type != Net_type::Tnet && record.type != Net_type::PNnet) {
            continue;
        }
        for (const auto& b : record.start_bumps) {
            ensure_qs_qw(b);
            if (record.type == Net_type::Tnet) {
                add_u_equals_z_rows(n, b, "UEQ_T");
            }
            else {
                add_u_equals_z_rows(n, b, "UEQ_P");
            }
        }
    }
}

} // namespace PR_tool
