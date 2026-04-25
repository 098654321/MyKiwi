// Build ILP model from config, solve with HiGHS, optional MPS export.

#include "highs.hh"
#include "ilp_types.hh"
#include "tob_ilp_model.hh"

#include <algo/netbuilder/netbuilder.hh>
#include <circuit/net/types/bbnet.hh>
#include <circuit/net/types/bbsnet.hh>
#include <circuit/net/types/btnet.hh>
#include <circuit/net/types/btsnet.hh>
#include <circuit/net/types/syncnet.hh>
#include <circuit/net/types/tbnet.hh>
#include <circuit/net/types/tbsnet.hh>
#include <circuit/net/types/tsbsnet.hh>
#include <debug/debug.hh>
#include <hardware/interposer.hh>
#include <parse/reader/module.hh>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <limits>
#include <format>
#include <map>
#include <sys/resource.h>
#include <stdexcept>
#include <string>
#include <tuple>

namespace PR_tool {

auto bump_to_ilp_coord(const hardware::Bump* bump) -> Bump_coord;
auto sort_and_unique(std::Vector<std::size_t>& values) -> void;
auto sort_and_unique(std::Vector<Bump_coord>& values) -> void;
auto classify_net(const std::Rc<circuit::Net>& net) -> Net_cost_record;
auto build_records(const std::Vector<std::Rc<circuit::Net>>& nets) -> std::Vector<Net_cost_record>;
auto build_cost_matrix(const std::Vector<Net_cost_record>& records) -> std::Vector<Net_cost_matrix>;
auto write_mps_file(
    const std::Vector<Net_cost_record>& records,
    const std::Vector<Net_cost_matrix>& costs,
    const std::String& output_mps,
    bool enable_objective
) -> void;
auto get_peak_rss_mb() -> double;

auto run_main(int argc, char** argv) -> int {
    if (argc < 2) {
        debug::error("No config path given");
        debug::info("Usage: xmake run test_ILP <config_path> [output_mps_path] [--enable-objective]");
        return 1;
    }

    const auto config_path = std::String(argv[1]);
    auto output_mps = std::String {};
    bool enable_objective = false;
    for (int argi = 2; argi < argc; ++argi) {
        const auto arg = std::String(argv[argi]);
        if (arg == "--enable-objective") {
            enable_objective = true;
            continue;
        }
        if (output_mps.empty()) {
            output_mps = arg;
            continue;
        }
        debug::error_fmt("Unexpected argument '{}'", arg);
        debug::info("Usage: xmake run test_ILP <config_path> [output_mps_path] [--enable-objective]");
        return 1;
    }

    debug::initial_log("./debug.log");
    auto [interposer, basedie] = PR_tool::parse::read_config(config_path, 0, false);
    algo::build_nets(basedie.get(), interposer.get());

    const auto nets = basedie->nets_to_vector();
    const auto records = build_records(nets);
    const auto costs = build_cost_matrix(records);

    if (!output_mps.empty()) {
        write_mps_file(records, costs, output_mps, enable_objective);
        debug::info_fmt("MPS written: {}", output_mps);
    }

    const auto solve_begin = std::chrono::steady_clock::now();
    const auto result = solve_tob_ilp_with_highs(records, costs, enable_objective);
    const auto solve_end = std::chrono::steady_clock::now();
    const auto solve_ms = std::chrono::duration_cast<std::chrono::milliseconds>(solve_end - solve_begin).count();
    const auto peak_rss_mb = get_peak_rss_mb();
    debug::info_fmt("ILP solve elapsed: {} ms", solve_ms);
    debug::info_fmt("Process peak RSS: {:.2f} MB", peak_rss_mb);

    if (!result.ok) {
        debug::error_fmt("HiGHS: {}", result.message);
        return 1;
    }
    for (const auto& a : result.assignments) {
        debug::info_fmt("net \"{}\" -> COBUnit {}", a.net_name, a.cob_unit);
    }
    for (const auto& d : result.route_details) {
        debug::info_fmt(
            "net \"{}\": bump(T{},B{},G{},I{}) -> j={}, k={}, s={}, orient={}, track={}, COBUnit={}",
            d.net_name,
            d.bump.TOB,
            d.bump.Bank,
            d.bump.Group,
            d.bump.Index,
            d.j,
            d.k,
            d.s_v,
            d.use_straight ? "straight(QS)" : "wrap(QW)",
            d.track,
            d.cob_unit
        );
    }
    debug::info_fmt("active W count: {}", result.active_w.size());
    for (const auto& w : result.active_w) {
        if (!w.has_track) {
            debug::info_fmt(
                "W active: bump(T{},B{},G{},I{}) j={}, k={} (track unresolved)",
                w.bump.TOB,
                w.bump.Bank,
                w.bump.Group,
                w.bump.Index,
                w.j,
                w.k
            );
            continue;
        }
        debug::info_fmt(
            "W active: bump(T{},B{},G{},I{}) j={}, k={}, orient={}, track={}",
            w.bump.TOB,
            w.bump.Bank,
            w.bump.Group,
            w.bump.Index,
            w.j,
            w.k,
            w.use_straight ? "straight" : "wrap",
            w.track
        );
    }
    debug::info_fmt("active S count: {}", result.active_s.size());
    for (const auto& s : result.active_s) {
        debug::info_fmt("S active: TOB={}, v={} (j={}, k={})", s.tob, s.v, s.j, s.k);
    }
    debug::info_fmt("objective value: {}", result.objective);
    debug::info_fmt("nets solved: {}", records.size());
    return 0;
}

auto bump_to_ilp_coord(const hardware::Bump* bump) -> Bump_coord {
    const auto bump_index = bump->index();
    const auto tob_coord = bump->tob()->coord();
    return Bump_coord {
        static_cast<std::size_t>(tob_coord.row * hardware::Interposer::TOB_ARRAY_WIDTH + tob_coord.col),
        bump_index / 64,
        (bump_index % 64) / 8,
        bump_index % 8
    };
}

auto sort_and_unique(std::Vector<std::size_t>& values) -> void {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

auto sort_and_unique(std::Vector<Bump_coord>& values) -> void {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

auto classify_net(const std::Rc<circuit::Net>& net) -> Net_cost_record {
    const auto add_all_cobunits = [](std::Vector<std::size_t>& cs) {
        for (std::size_t c = 0; c < 16; ++c) {
            cs.emplace_back(c);
        }
    };

    Net_cost_record record {
        net->name(),
        Net_type::Tnet,
        static_cast<float>(net->port_number()) / 2.0F,
        1.0F,
        {},
        {},
        {},
        {}
    };

    if (record.bits <= 0.0F) {
        throw std::runtime_error(std::format("net '{}' has non-positive bits", net->name()));
    }

    if (const auto* bb_net = dynamic_cast<const circuit::BumpToBumpNet*>(net.get())) {
        record.type = Net_type::Bnet;
        record.lambda = 10.0F;
        record.start_bumps.emplace_back(bump_to_ilp_coord(bb_net->begin_bump()));
        record.end_bumps.emplace_back(bump_to_ilp_coord(bb_net->end_bump()));
        add_all_cobunits(record.candidate_cobunits);
    }
    else if (dynamic_cast<const circuit::BumpToBumpsNet*>(net.get()) != nullptr) {
        throw std::runtime_error(std::format("unsupported net type BumpToBumpsNet: '{}'", net->name()));
    }
    else if (const auto* bt_net = dynamic_cast<const circuit::BumpToTrackNet*>(net.get())) {
        const auto cobunit = map_track(bt_net->end_track()->coord().index);
        record.type = Net_type::Tnet;
        record.lambda = 1.0F;
        record.start_bumps.emplace_back(bump_to_ilp_coord(bt_net->begin_bump()));
        record.candidate_cobunits.emplace_back(cobunit);
        record.tnet_fixed_cobunits.emplace_back(cobunit);
    }
    else if (dynamic_cast<const circuit::BumpToTracksNet*>(net.get()) != nullptr) {
        throw std::runtime_error(std::format("unsupported net type BumpToTracksNet: '{}'", net->name()));
    }
    else if (const auto* tb_net = dynamic_cast<const circuit::TrackToBumpNet*>(net.get())) {
        const auto cobunit = map_track(tb_net->begin_track()->coord().index);
        record.type = Net_type::Tnet;
        record.lambda = 1.0F;
        record.start_bumps.emplace_back(bump_to_ilp_coord(tb_net->end_bump()));
        record.candidate_cobunits.emplace_back(cobunit);
        record.tnet_fixed_cobunits.emplace_back(cobunit);
    }
    else if (dynamic_cast<const circuit::TrackToBumpsNet*>(net.get()) != nullptr) {
        throw std::runtime_error(std::format("unsupported net type TrackToBumpsNet: '{}'", net->name()));
    }
    else if (dynamic_cast<const circuit::TracksToBumpsNet*>(net.get()) != nullptr) {
        throw std::runtime_error(std::format("TracksToBumpsNet should be expanded in build_records: '{}'", net->name()));
    }
    else if (dynamic_cast<circuit::SyncNet*>(net.get()) != nullptr) {
        throw std::runtime_error(std::format("SyncNet should be expanded in build_records: '{}'", net->name()));
    }
    else {
        throw std::runtime_error(std::format("unknown net type: '{}'", net->name()));
    }

    sort_and_unique(record.start_bumps);
    sort_and_unique(record.end_bumps);
    sort_and_unique(record.candidate_cobunits);
    sort_and_unique(record.tnet_fixed_cobunits);

    if (record.start_bumps.empty()) {
        throw std::runtime_error(std::format("net '{}' has empty start bump set", net->name()));
    }
    if (record.candidate_cobunits.empty()) {
        throw std::runtime_error(std::format("net '{}' has empty candidate cobunits", net->name()));
    }
    return record;
}

auto build_records(const std::Vector<std::Rc<circuit::Net>>& nets) -> std::Vector<Net_cost_record> {
    auto records = std::Vector<Net_cost_record> {};
    records.reserve(nets.size());
    for (const auto& net : nets) {
        if (const auto* tsb_net = dynamic_cast<const circuit::TracksToBumpsNet*>(net.get())) {
            // Split one TracksToBumpsNet into multiple "bump -> tracks" pseudo nets.
            auto candidate_cobunits = std::Vector<std::size_t> {};
            for (auto* track : tsb_net->begin_tracks()) {
                candidate_cobunits.emplace_back(map_track(track->coord().index));
            }
            sort_and_unique(candidate_cobunits);
            for (auto* bump : tsb_net->end_bumps()) {
                Net_cost_record record {
                    std::String(std::format("{}__split_{}", net->name(), records.size())),
                    Net_type::PNnet,
                    static_cast<float>(net->port_number()) / 2.0F,
                    5.0F,
                    {bump_to_ilp_coord(bump)},
                    {},
                    candidate_cobunits,
                    {}
                };
                records.emplace_back(std::move(record));
            }
            continue;
        }

        if (auto* sync_net = dynamic_cast<circuit::SyncNet*>(net.get())) {
            // Split SyncNet into independent 2-pin nets for ILP modeling.
            for (const auto& btb : sync_net->btbnets()) {
                Net_cost_record record {
                    std::String(std::format("{}__btb_{}", net->name(), records.size())),
                    Net_type::Bnet,
                    static_cast<float>(btb->port_number()) / 2.0F,
                    10.0F,
                    {bump_to_ilp_coord(btb->begin_bump())},
                    {bump_to_ilp_coord(btb->end_bump())},
                    {},
                    {}
                };
                for (std::size_t c = 0; c < 16; ++c) {
                    record.candidate_cobunits.emplace_back(c);
                }
                records.emplace_back(std::move(record));
            }
            for (const auto& btt : sync_net->bttnets()) {
                const auto cobunit = map_track(btt->end_track()->coord().index);
                Net_cost_record record {
                    std::String(std::format("{}__btt_{}", net->name(), records.size())),
                    Net_type::Tnet,
                    static_cast<float>(btt->port_number()) / 2.0F,
                    1.0F,
                    {bump_to_ilp_coord(btt->begin_bump())},
                    {},
                    {cobunit},
                    {cobunit}
                };
                records.emplace_back(std::move(record));
            }
            for (const auto& ttb : sync_net->ttbnets()) {
                const auto cobunit = map_track(ttb->begin_track()->coord().index);
                Net_cost_record record {
                    std::String(std::format("{}__ttb_{}", net->name(), records.size())),
                    Net_type::Tnet,
                    static_cast<float>(ttb->port_number()) / 2.0F,
                    1.0F,
                    {bump_to_ilp_coord(ttb->end_bump())},
                    {},
                    {cobunit},
                    {cobunit}
                };
                records.emplace_back(std::move(record));
            }
            continue;
        }

        // for other net types, classify them into Net_cost_record
        records.emplace_back(classify_net(net));
    }

    // show number of all net types
    std::size_t bnet_count = 0;
    std::size_t pnnet_count = 0;
    std::size_t tnet_count = 0;
    for (const auto& record : records) {
        if (record.type == Net_type::Bnet) {
            ++bnet_count;
        }
        else if (record.type == Net_type::PNnet) {
            ++pnnet_count;
        }
        else if (record.type == Net_type::Tnet) {
            ++tnet_count;
        }
    }
    debug::info_fmt("number of Bnet: {}", bnet_count);
    debug::info_fmt("number of PNnet: {}", pnnet_count);
    debug::info_fmt("number of Tnet: {}", tnet_count);
    debug::info_fmt("total number of nets: {}", records.size());

    return records;
}

auto build_cost_matrix(const std::Vector<Net_cost_record>& records) -> std::Vector<Net_cost_matrix> {
    auto load = std::array<double, 16> {};
    load.fill(0.0);

    for (const auto& record : records) {
        if (record.type == Net_type::Bnet) {
            const auto share = static_cast<double>(record.bits) / 16.0;
            for (std::size_t c = 0; c < 16; ++c) {
                load[c] += share;
            }
            continue;
        }

        if (record.type == Net_type::PNnet) {
            const auto unit_num = static_cast<double>(record.candidate_cobunits.size());
            const auto share = static_cast<double>(record.bits) / unit_num;
            for (const auto cobunit : record.candidate_cobunits) {
                load[cobunit] += share;
            }
            continue;
        }

        // Tnet
        const auto& fixed = record.tnet_fixed_cobunits.empty() ? record.candidate_cobunits : record.tnet_fixed_cobunits;
        const auto unit_num = static_cast<double>(fixed.size());
        const auto share = static_cast<double>(record.bits) / unit_num;
        for (const auto cobunit : fixed) {
            load[cobunit] += share;
        }
    }

    // Per-COB load thresholds for piecewise cost: default +inf (linear Cost = load*lambda only).
    static constexpr std::array<double, 16> kCobLoadThresholds = [] {
        std::array<double, 16> t {};
        t.fill(std::numeric_limits<double>::infinity());
        return t;
    }();
    static constexpr double kLoadAboveThresholdMultiplier = 100.0;

    auto costs = std::Vector<Net_cost_matrix> {};
    costs.reserve(records.size());
    for (const auto& record : records) {
        auto per_net = Net_cost_matrix {};
        for (const auto& bump : record.start_bumps) {
            auto row = Bump_cost_row {};
            row.fill(0.0);
            for (std::size_t c = 0; c < 16; ++c) {
                const double m = (load[c] > kCobLoadThresholds[c]) ? kLoadAboveThresholdMultiplier : 1.0;
                row[c] = load[c] * static_cast<double>(record.lambda) * m;
            }
            per_net.emplace(bump, row);
        }
        costs.emplace_back(std::move(per_net));
    }
    return costs;
}

// MARK: write mps file (optional; same model as HiGHS)
auto write_mps_file(
    const std::Vector<Net_cost_record>& records,
    const std::Vector<Net_cost_matrix>& costs,
    const std::String& output_mps,
    const bool enable_objective
) -> void {
    TobIlpModel m {};
    build_tob_ilp_model(m, records, costs, enable_objective);
    m.write_mps(output_mps);
}
// MARK: END of writing mps file

auto get_peak_rss_mb() -> double {
    struct rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return -1.0;
    }
#if defined(__APPLE__)
    // macOS reports ru_maxrss in bytes.
    constexpr double kBytesPerMb = 1024.0 * 1024.0;
    return static_cast<double>(usage.ru_maxrss) / kBytesPerMb;
#else
    // Linux reports ru_maxrss in kilobytes.
    constexpr double kKbPerMb = 1024.0;
    return static_cast<double>(usage.ru_maxrss) / kKbPerMb;
#endif
}

} // namespace PR_tool

auto main(int argc, char** argv) -> int {
    return PR_tool::run_main(argc, argv);
}
