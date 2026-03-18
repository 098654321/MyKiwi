#include "./route_multi_mode.hh"

#include <circuit/basedie.hh>
#include <circuit/net/net.hh>
#include <algo/router/common/maze/mazeroutestrategy.hh>
#include <algo/router/single_mode/incremental/recorders/hardware_recorder.hh>
#include <debug/debug.hh>
#include <parse/writer/module.hh>

#include "./occupancy_view.hh"
#include "./hungarian.hh"
#include "./params.hh"
#include "./entry_exit.hh"
#include "./paired_iterative.hh"

namespace kiwi::algo {

    auto route_multi_mode(
        hardware::Interposer* interposer,
        circuit::BaseDie* basedie,
        std::StringView /*config_path*/,
        const std::FilePath& output_path,
        const kiwi::algo::multi_mode::MultiModeParams& params
    ) -> void {
        auto& nets_map = basedie->nets();
        auto it1 = nets_map.find(1);
        auto it2 = nets_map.find(2);
        if (it1 == nets_map.end() || it2 == nets_map.end()) {
            debug::fatal("multi-mode routing expects mode 1 and mode 2 nets");
        }

        auto view = kiwi::algo::multi_mode::OccupancyView{interposer};
        auto recorder = HardwareRecorder{interposer};

        std::usize shared = 0;
        std::usize only1 = 0;
        std::usize only2 = 0;

        for (const auto& net_rc : it1->second) {
            auto* net = net_rc.get();
            if (net == nullptr) { continue; }
            if (net->modes().size() > 1) { ++shared; }
            else { ++only1; }
        }
        for (const auto& net_rc : it2->second) {
            auto* net = net_rc.get();
            if (net == nullptr) { continue; }
            if (net->modes().size() > 1) { continue; } // already counted as shared via mode1 view
            ++only2;
        }

        debug::info_fmt(
            "multi-mode nets classified: shared={}, mode1_only={}, mode2_only={}",
            shared, only1, only2
        );

        // Step E: route shared nets first (non-incremental maze), lock their occupancy for both modes.
        auto maze = algo::MazeRouteStrategy{false};
        std::usize shared_routed = 0;
        std::usize shared_total_len = 0;
        for (const auto& net_rc : it1->second) {
            auto* net = net_rc.get();
            if (net == nullptr) { continue; }
            if (net->modes().size() <= 1) { continue; }

            net->route(interposer, maze);
            shared_routed += 1;
            shared_total_len += net->length();

            auto& pkg = net->pathpackage();
            // lock COB occupancy in both modes
            for (auto& [t, cob] : pkg._regular_path) {
                (void)t;
                if (cob.has_value()) {
                    view.occupy_cobconnector(1, cob.value(), true);
                    view.occupy_cobconnector(2, cob.value(), true);
                }
            }
            // lock TOB mux occupancy in both modes
            for (auto& [b, tob, t] : pkg._tob_to_track) {
                (void)b; (void)t;
                view.occupy_tobconnector(1, tob);
                view.occupy_tobconnector(2, tob);
            }
            for (auto& [b, tob, t] : pkg._track_to_tob) {
                (void)b; (void)t;
                view.occupy_tobconnector(1, tob);
                view.occupy_tobconnector(2, tob);
            }

            recorder.update_recorders_current(pkg, true);
            recorder.update_recorders_history(pkg, true);
        }
        debug::info_fmt("shared nets routed: count={}, total_length={}", shared_routed, shared_total_len);

        // Step F/G: compute bounding boxes and run Hungarian matching on non-shared nets.
        auto boxes1 = std::Vector<circuit::BoundingBox>{};
        auto boxes2 = std::Vector<circuit::BoundingBox>{};
        auto no_bbox_mode1 = std::Vector<circuit::Net*>{};
        auto no_bbox_mode2 = std::Vector<circuit::Net*>{};

        for (const auto& net_rc : it1->second) {
            auto* net = net_rc.get();
            if (net == nullptr) { continue; }
            if (net->modes().size() > 1) { continue; } // shared already routed
            auto bb = net->compute_bounding_box(1);
            if (bb.has_value()) {
                boxes1.emplace_back(bb.value());
            } else {
                no_bbox_mode1.emplace_back(net);
            }
        }
        for (const auto& net_rc : it2->second) {
            auto* net = net_rc.get();
            if (net == nullptr) { continue; }
            if (net->modes().size() > 1) { continue; }
            auto bb = net->compute_bounding_box(2);
            if (bb.has_value()) {
                boxes2.emplace_back(bb.value());
            } else {
                no_bbox_mode2.emplace_back(net);
            }
        }

        const auto n = std::max(boxes1.size(), boxes2.size());
        if (n == 0) {
            debug::fatal("no non-shared nets with bounding boxes found");
        }

        auto weights = std::Vector<std::Vector<std::i64>>{};
        weights.resize(n);
        for (std::usize i = 0; i < n; ++i) {
            weights[i] = std::Vector<std::i64>(n, 0);
        }

        auto overlap = [](const circuit::Region& a, const circuit::Region& b) -> std::Option<circuit::Region> {
            auto r = circuit::Region{};
            r.row_min = std::max(a.row_min, b.row_min);
            r.row_max = std::min(a.row_max, b.row_max);
            r.col_min = std::max(a.col_min, b.col_min);
            r.col_max = std::min(a.col_max, b.col_max);
            if (r.row_min > r.row_max || r.col_min > r.col_max) {
                return std::nullopt;
            }
            return r;
        };

        auto overlap_cob_count = [](circuit::Region r) -> std::i64 {
            r.normalize();
            r.row_min = std::max<std::i64>(0, r.row_min);
            r.col_min = std::max<std::i64>(0, r.col_min);
            r.row_max = std::min<std::i64>(hardware::Interposer::COB_ARRAY_HEIGHT - 1, r.row_max);
            r.col_max = std::min<std::i64>(hardware::Interposer::COB_ARRAY_WIDTH - 1, r.col_max);
            if (r.row_min > r.row_max || r.col_min > r.col_max) {
                return 0;
            }
            return (r.row_max - r.row_min + 1) * (r.col_max - r.col_min + 1);
        };

        for (std::usize i = 0; i < n; ++i) {
            for (std::usize j = 0; j < n; ++j) {
                if (i >= boxes1.size() || j >= boxes2.size()) {
                    weights[i][j] = 0; // dummy boxes
                    continue;
                }
                auto ov = overlap(boxes1[i].region, boxes2[j].region);
                if (!ov.has_value()) {
                    weights[i][j] = 0;
                } else {
                    weights[i][j] = overlap_cob_count(ov.value());
                }
            }
        }

        auto assign = kiwi::algo::multi_mode::hungarian_max_weight(weights);
        std::usize paired = 0;
        std::i64 paired_weight_sum = 0;
        std::usize matched_to_dummy = 0;
        std::usize matched_no_overlap = 0;

        struct PairedNetGroup {
            circuit::Net* net1{nullptr};
            circuit::Net* net2{nullptr};
            circuit::Region overlap{};
            double priority_sum{0.0};
        };
        auto pairs = std::Vector<PairedNetGroup>{};
        auto unpaired_mode1 = std::Vector<circuit::Net*>{};
        auto unpaired_mode2 = std::Vector<circuit::Net*>{};

        for (std::usize i = 0; i < boxes1.size(); ++i) {
            const auto j = assign[i];
            if (j >= boxes2.size()) {
                ++matched_to_dummy;
                unpaired_mode1.emplace_back(boxes1[i].net);
                continue; // matched to dummy
            }
            auto ov = overlap(boxes1[i].region, boxes2[j].region);
            if (!ov.has_value()) {
                ++matched_no_overlap;
                unpaired_mode1.emplace_back(boxes1[i].net);
                unpaired_mode2.emplace_back(boxes2[j].net);
                continue;
            }
            paired_weight_sum += weights[i][j];
            // record on Net objects for next phase
            boxes1[i].matched_net = boxes2[j].net;
            boxes1[i].overlap_region = ov;
            boxes2[j].matched_net = boxes1[i].net;
            boxes2[j].overlap_region = ov;
            ++paired;

            auto p = PairedNetGroup{};
            p.net1 = boxes1[i].net;
            p.net2 = boxes2[j].net;
            p.overlap = ov.value();
            p.priority_sum = static_cast<double>(p.net1->priority().value()) + static_cast<double>(p.net2->priority().value());
            pairs.emplace_back(std::move(p));
        }

        debug::info_fmt(
            "hungarian matching done: mode1_boxes={}, mode2_boxes={}, paired={}, matched_to_dummy={}, matched_no_overlap={}",
            boxes1.size(), boxes2.size(), paired, matched_to_dummy, matched_no_overlap
        );
        if (paired > 0) {
            debug::info_fmt("matching avg_overlap_weight={}", static_cast<double>(paired_weight_sum) / static_cast<double>(paired));
        }

        debug::info_fmt(
            "multi-mode params: k_candidates={}, converge_threshold={}",
            params.k_candidates, params.converge_threshold
        );

        // Step 5: build paired net groups (sorted by priority sum, lower is higher priority).
        std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
            return a.priority_sum < b.priority_sum;
        });

        debug::info_fmt(
            "paired net groups prepared: pairs={}, no_bbox_mode1={}, no_bbox_mode2={}",
            pairs.size(), no_bbox_mode1.size(), no_bbox_mode2.size()
        );

        // Step 5: route paired groups with k-parallel iterative solver and commit winner.
        auto failed = std::Vector<circuit::Net*>{};
        std::usize paired_success = 0;
        for (const auto& p : pairs) {
            auto res = kiwi::algo::multi_mode::route_paired_nets_iterative(
                interposer, view, recorder,
                *p.net1, 1,
                *p.net2, 2,
                p.overlap, params
            );
            if (!res.success) {
                failed.emplace_back(p.net1);
                failed.emplace_back(p.net2);
                continue;
            }

            // Commit to hardware/pathpackage in main thread.
            {
                auto pkg1 = circuit::PathPackage{res.net1_history, interposer};
                pkg1.occupy_all();
                p.net1->set_pathpackage(pkg1);
                recorder.update_recorders_current(pkg1, false);
                recorder.update_recorders_history(pkg1, false);
                for (auto& [t, cob] : pkg1._regular_path) {
                    (void)t;
                    if (cob.has_value()) { view.occupy_cobconnector(1, cob.value(), false); }
                }
                for (auto& [b, tob, t] : pkg1._tob_to_track) { (void)b; (void)t; view.occupy_tobconnector(1, tob); }
                for (auto& [b, tob, t] : pkg1._track_to_tob) { (void)b; (void)t; view.occupy_tobconnector(1, tob); }
            }
            {
                auto pkg2 = circuit::PathPackage{res.net2_history, interposer};
                pkg2.occupy_all();
                p.net2->set_pathpackage(pkg2);
                recorder.update_recorders_current(pkg2, false);
                recorder.update_recorders_history(pkg2, false);
                for (auto& [t, cob] : pkg2._regular_path) {
                    (void)t;
                    if (cob.has_value()) { view.occupy_cobconnector(2, cob.value(), false); }
                }
                for (auto& [b, tob, t] : pkg2._tob_to_track) { (void)b; (void)t; view.occupy_tobconnector(2, tob); }
                for (auto& [b, tob, t] : pkg2._track_to_tob) { (void)b; (void)t; view.occupy_tobconnector(2, tob); }
            }
            paired_success += 1;
        }
        debug::info_fmt("paired routing done: success={}, failed_pairs={}", paired_success, pairs.size() - paired_success);

        // Step 6: route remaining nets (unpaired + no-bbox + paired-failed) using existing non-incremental maze.
        auto remaining1 = std::Vector<circuit::Net*>{};
        remaining1.insert(remaining1.end(), no_bbox_mode1.begin(), no_bbox_mode1.end());
        remaining1.insert(remaining1.end(), unpaired_mode1.begin(), unpaired_mode1.end());

        auto remaining2 = std::Vector<circuit::Net*>{};
        remaining2.insert(remaining2.end(), no_bbox_mode2.begin(), no_bbox_mode2.end());
        remaining2.insert(remaining2.end(), unpaired_mode2.begin(), unpaired_mode2.end());

        // De-dup (best-effort).
        auto dedup = [](std::Vector<circuit::Net*>& v) {
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
        };
        dedup(remaining1);
        dedup(remaining2);

        auto maze2 = algo::MazeRouteStrategy{false};
        for (auto* net : remaining1) {
            if (net == nullptr) { continue; }
            if (net->modes().size() > 1) { continue; }
            net->route(interposer, maze2);
            auto& pkg = net->pathpackage();
            for (auto& [t, cob] : pkg._regular_path) {
                (void)t;
                if (cob.has_value()) { view.occupy_cobconnector(1, cob.value(), false); }
            }
            for (auto& [b, tob, t] : pkg._tob_to_track) { (void)b; (void)t; view.occupy_tobconnector(1, tob); }
            for (auto& [b, tob, t] : pkg._track_to_tob) { (void)b; (void)t; view.occupy_tobconnector(1, tob); }
            recorder.update_recorders_current(pkg, false);
            recorder.update_recorders_history(pkg, false);
        }
        for (auto* net : remaining2) {
            if (net == nullptr) { continue; }
            if (net->modes().size() > 1) { continue; }
            net->route(interposer, maze2);
            auto& pkg = net->pathpackage();
            for (auto& [t, cob] : pkg._regular_path) {
                (void)t;
                if (cob.has_value()) { view.occupy_cobconnector(2, cob.value(), false); }
            }
            for (auto& [b, tob, t] : pkg._tob_to_track) { (void)b; (void)t; view.occupy_tobconnector(2, tob); }
            for (auto& [b, tob, t] : pkg._track_to_tob) { (void)b; (void)t; view.occupy_tobconnector(2, tob); }
            recorder.update_recorders_current(pkg, false);
            recorder.update_recorders_history(pkg, false);
        }

        // Step 7: output both modes.
        debug::info_fmt(
            "multi-mode stats: shared_count={}, shared_total_length={}, paired_total={}, paired_success={}, remaining_mode1={}, remaining_mode2={}",
            shared_routed, shared_total_len, pairs.size(), paired_success, remaining1.size(), remaining2.size()
        );
        parse::output_two_modes_from_routing_results(interposer, output_path, basedie, 1, 2);
    }

}

