#include "./route_multi_mode.hh"

#include <circuit/basedie.hh>
#include <circuit/net/net.hh>
#include <algo/router/common/maze/mazeroutestrategy.hh>
#include <algo/router/single_mode/incremental/recorders/hardware_recorder.hh>
#include <debug/debug.hh>
#include <parse/writer/module.hh>
#include <stdexcept>
#include <algorithm>
#include <std/format.hh>

#include "./occupancy_view.hh"
#include "./hungarian.hh"
#include "./params.hh"
#include "./entry_exit.hh"
#include "./paired_iterative.hh"
#include <circuit/net/types/syncnet.hh>

namespace kiwi::algo {

    static auto commit_paired_net_result(
        hardware::Interposer* interposer,
        OccupancyView& view,
        HardwareRecorder& recorder,
        int mode,
        circuit::Net* net,
        const NetRouteHistoryResult& result
    ) -> void {
        if (net == nullptr) {
            throw std::logic_error("commit_paired_net_result(): net is nullptr");
        }
        if (result.subnet_histories.empty()) {
            throw std::logic_error(std::format(
                "commit_paired_net_result(): empty subnet histories for net='{}'",
                net->name()
            ));
        }

        if (auto* sync_net = dynamic_cast<circuit::SyncNet*>(net)) {
            const auto expected = sync_net->btbnets().size() + sync_net->ttbnets().size() + sync_net->bttnets().size();
            if (result.subnet_histories.size() != expected) {
                throw std::logic_error(std::format(
                    "commit_paired_net_result(): sync subnet count mismatch for net='{}', expect={}, got={}",
                    net->name(),
                    expected,
                    result.subnet_histories.size()
                ));
            }
            std::usize idx = 0;
            auto apply_next = [&](circuit::Net* sub_net) {
                if (idx >= result.subnet_histories.size()) {
                    throw std::logic_error("commit_paired_net_result(): insufficient subnet histories");
                }
                auto sub_pkg = circuit::PathPackage{result.subnet_histories[idx], interposer};
                sub_net->set_pathpackage(sub_pkg);
                idx += 1;
            };
            for (auto& sub : sync_net->btbnets()) { apply_next(sub.get()); }
            for (auto& sub : sync_net->ttbnets()) { apply_next(sub.get()); }
            for (auto& sub : sync_net->bttnets()) { apply_next(sub.get()); }
            if (idx != result.subnet_histories.size()) {
                throw std::logic_error("commit_paired_net_result(): unexpected extra subnet histories");
            }
            sync_net->collect_package();
        } else {
            if (result.subnet_histories.size() != 1) {
                throw std::logic_error(std::format(
                    "commit_paired_net_result(): non-sync net='{}' expects one subnet history, got={}",
                    net->name(),
                    result.subnet_histories.size()
                ));
            }
            auto pkg = circuit::PathPackage{result.subnet_histories.front(), interposer};
            net->set_pathpackage(pkg);
        }

        auto& pkg = net->pathpackage();
        // pkg.occupy_all();
        net->show_path();
        recorder.update_recorders_history(pkg, false);
        for (auto& [t, cob] : pkg._regular_path) {
            (void)t;
            if (cob.has_value()) { view.occupy_cobconnector(mode, cob.value(), false); }
        }
        for (auto& [b, tob, t] : pkg._tob_to_track) { (void)b; (void)t; view.occupy_tobconnector(mode, tob); }
        for (auto& [b, tob, t] : pkg._track_to_tob) { (void)b; (void)t; view.occupy_tobconnector(mode, tob); }
    }

    static auto format_region(const circuit::Region& region) -> std::String {
        return std::format(
            "rows=[{},{}], cols=[{},{}]",
            region.row_min,
            region.row_max,
            region.col_min,
            region.col_max
        );
    }

    auto route_shared_nets(
        OccupancyView& view,
        HardwareRecorder& recorder,
        hardware::Interposer* interposer,
        const std::vector<std::shared_ptr<circuit::Net>>& shared
    ) -> void {
        try {
            auto sorted_shared = sort_shared_nets(shared);
            auto [shared_total_len, shared_routed] = route_shared(view, recorder, interposer, sorted_shared);
            (void)shared_total_len;
            (void)shared_routed;
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format(
                "route_shared_nets() failed: shared_count={}, reason={}",
                shared.size(),
                err.what()
            ));
        }
    }

    auto route_mode_only_nets(
        OccupancyView& view,
        HardwareRecorder& recorder,
        hardware::Interposer* interposer,
        const NetsMapConstIter& it1,
        const NetsMapConstIter& it2,
        const MultiModeParams& params
    ) -> void {
        try {
            auto [boxes1, boxes2, no_bbox_mode1, no_bbox_mode2] = compute_bounding_boxes(it1, it2);
            const auto n = std::max(boxes1.size(), boxes2.size());
            if (n == 0) {
                throw std::logic_error(std::format(
                    "route_mode_only_nets(): no non-shared nets with bounding boxes; mode1_total_nets={}, mode2_total_nets={}, mode1_no_bbox={}, mode2_no_bbox={}",
                    it1->second.size(),
                    it2->second.size(),
                    no_bbox_mode1.size(),
                    no_bbox_mode2.size()
                ));
            }
            auto weights = compute_overlap_weights(boxes1, boxes2, n);
            auto assign = kiwi::algo::hungarian_max_weight(weights);
            auto [pairs, unpaired_mode1, unpaired_mode2] = match_nets(boxes1, boxes2, assign, weights);
            debug::info_fmt(
                "multi-mode params: k_candidates={}, converge_threshold={}",
                params.k_candidates, params.converge_threshold
            );

            std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
                return a.priority_sum < b.priority_sum;
            });
            debug::info_fmt(
                "paired net groups prepared: pairs={}, no_bbox_mode1={}, no_bbox_mode2={}",
                pairs.size(), no_bbox_mode1.size(), no_bbox_mode2.size()
            );
            auto [paired_success, failed] = route_paired(interposer, view, recorder, pairs, params);
            if (!failed.empty()) {
                debug::warning_fmt(
                    "route_mode_only_nets(): paired routing fallback pending for {} nets after paired_success={}.",
                    failed.size(),
                    paired_success
                );
            }

            // route remaining nets
            auto remaining1 = collect_remaining_nets(no_bbox_mode1, unpaired_mode1);
            auto remaining2 = collect_remaining_nets(no_bbox_mode2, unpaired_mode2);
            auto maze2 = algo::MazeRouteStrategy{false};
            route_remaining_nets(interposer, view, recorder, 1, remaining1, maze2);
            route_remaining_nets(interposer, view, recorder, 2, remaining2, maze2);
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format(
                "route_mode_only_nets() failed: mode1_nets={}, mode2_nets={}, reason={}",
                it1->second.size(),
                it2->second.size(),
                err.what()
            ));
        }
    }

    auto set_resources(
        const NetsMapConstIter& it1,
        const NetsMapConstIter& it2
    ) -> void {
        for (const auto& net : it1->second) {
            net->check_accessable_cobunit();
        }
        for (const auto& net : it2->second) {
            net->check_accessable_cobunit();
        }
    }

    // return [shared, mode1_only, mode2_only]
    auto classify_nets(
        const NetsMapConstIter& mode1_it,
        const NetsMapConstIter& mode2_it
    ) -> std::Tuple<std::vector<std::shared_ptr<circuit::Net>>, std::vector<std::shared_ptr<circuit::Net>>, std::vector<std::shared_ptr<circuit::Net>>> {
        try {
            debug::info_fmt("classifying nets in mode 1 and mode 2");
            auto shared = std::vector<std::shared_ptr<circuit::Net>>{};
            auto only1 = std::vector<std::shared_ptr<circuit::Net>>{};
            auto only2 = std::vector<std::shared_ptr<circuit::Net>>{};
            auto shared_net_name = std::String{};
            auto only1_net_name = std::String{};
            auto only2_net_name = std::String{};

            for (std::usize i = 0; i < mode1_it->second.size(); ++i) {
                const auto& net = mode1_it->second[i];
                if (net == nullptr) {
                    throw std::logic_error(std::format("classify_nets(): nullptr net in mode 1 at index {}", i));
                }

                if (net->modes().size() > 1) {
                    if (std::find(shared.begin(), shared.end(), net) == shared.end()) {
                        shared.emplace_back(net);
                        shared_net_name += net->name() + "\n";
                    }
                }
                else {
                    only1.emplace_back(net);
                    only1_net_name += net->name() + "\n";
                }
            }
            for (std::usize i = 0; i < mode2_it->second.size(); ++i) {
                const auto& net = mode2_it->second[i];
                if (net == nullptr) {
                    throw std::logic_error(std::format("classify_nets(): nullptr net in mode 2 at index {}", i));
                }
                if (net->modes().size() > 1) {
                    if (std::find(shared.begin(), shared.end(), net) == shared.end()) {
                        shared.emplace_back(net);
                        shared_net_name += net->name() + "\n";
                    }
                }
                else {
                    if (std::find(only2.begin(), only2.end(), net) == only2.end()) {
                        only2.emplace_back(net);
                        only2_net_name += net->name() + "\n";
                    }
                }
            }

            debug::info_fmt(
                "multi-mode nets classified:\n{} shared nets:\n{}\n{} mode1_only nets:\n{}\n{} mode2_only nets:\n{}",
                shared.size(), shared_net_name, only1.size(), only1_net_name, only2.size(), only2_net_name
            );

            return std::make_tuple(std::move(shared), std::move(only1), std::move(only2));
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format("classify_nets() failed: {}", err.what()));
        }
    }

    auto sort_shared_nets(
        const std::vector<std::shared_ptr<circuit::Net>>& shared_nets
    ) -> std::vector<std::shared_ptr<circuit::Net>> {
        debug::info_fmt("sorting shared nets");

        auto shared_nets_vector = std::vector<std::shared_ptr<circuit::Net>>{shared_nets.begin(), shared_nets.end()};
        auto compare = [] (std::shared_ptr<circuit::Net> n1, std::shared_ptr<circuit::Net> n2) -> bool {
            return n1->priority() > n2->priority();
        };

        float max_port_num {0};
        for (const auto& net : shared_nets) {
            max_port_num = std::max(max_port_num, (float)net->port_number());
        }
        for (auto& net : shared_nets) {
            net->update_priority(0.9 * ((float)net->port_number() / max_port_num));
        }
        std::sort(shared_nets_vector.begin(), shared_nets_vector.end(), compare);

        return shared_nets_vector;
    }

    auto route_shared(
        OccupancyView& view,
        HardwareRecorder& recorder,
        hardware::Interposer* interposer,
        const std::vector<std::shared_ptr<circuit::Net>>& shared_nets
    ) -> std::tuple<float, float> {
        try {
            auto maze = algo::MazeRouteStrategy{false};
            debug::info_fmt("routing shared nets");

            std::Vector<circuit::Net*> routed_nets = {};
            std::usize shared_routed = 0;
            std::usize shared_total_len = 0;

            for (const auto& net: shared_nets) {
                try {
                    if (net == nullptr) {
                        throw std::logic_error("route_shared(): encountered nullptr shared net");
                    }

                    net->set_reuse_type(true);
                    net->search_related_nets(routed_nets);
                    net->route(interposer, maze);
                    shared_routed += 1;
                    shared_total_len += net->length();

                    auto& pkg = net->pathpackage();
                    for (auto& [t, cob] : pkg._regular_path) {
                        (void)t;
                        if (cob.has_value()) {
                            view.occupy_cobconnector(1, cob.value(), true);
                            view.occupy_cobconnector(2, cob.value(), true);
                        }
                    }
                    for (auto& [b, tob, t] : pkg._tob_to_track) {
                        (void)b; (void)t;
                        view.occupy_tobconnector(1, tob, true);
                        view.occupy_tobconnector(2, tob, true);
                    }
                    for (auto& [b, tob, t] : pkg._track_to_tob) {
                        (void)b; (void)t;
                        view.occupy_tobconnector(1, tob, true);
                        view.occupy_tobconnector(2, tob, true);
                    }

                    recorder.update_recorders_history(pkg, true);

                    routed_nets.emplace_back(net.get());
                }
                catch (const std::exception& err) {
                    throw std::runtime_error(std::format(
                        "route_shared(): failed on net='{}', reason={}",
                        net == nullptr ? std::String{"<null>"} : net->name(),
                        err.what()
                    ));
                }
            }

            debug::info_fmt("shared nets routed: count={}, total_length={}", shared_routed, shared_total_len);
            return std::make_tuple(shared_total_len, shared_routed);
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format("route_shared() failed: {}", err.what()));
        }
    }

    auto compute_bounding_boxes(
        const NetsMapConstIter& mode1_it,
        const NetsMapConstIter& mode2_it
    ) -> std::tuple<std::Vector<circuit::BoundingBox>, std::Vector<circuit::BoundingBox>, std::Vector<circuit::Net*>, std::Vector<circuit::Net*>> {
        try {
            debug::info_fmt("computing bounding boxes");

            auto boxes1 = std::Vector<circuit::BoundingBox>{};
            auto boxes2 = std::Vector<circuit::BoundingBox>{};
            auto no_bbox_mode1 = std::Vector<circuit::Net*>{};
            auto no_bbox_mode2 = std::Vector<circuit::Net*>{};

            for (std::usize i = 0; i < mode1_it->second.size(); ++i) {
                const auto& net_rc = mode1_it->second[i];
                auto* net = net_rc.get();
                if (net == nullptr) {
                    throw std::logic_error(std::format("compute_bounding_boxes(): nullptr net in mode 1 at index {}", i));
                }
                if (net->modes().size() > 1) { continue; }
                auto bb = net->compute_bounding_box(1);
                if (bb.has_value()) {
                    boxes1.emplace_back(bb.value());
                } else {
                    no_bbox_mode1.emplace_back(net);
                }
            }
            for (std::usize i = 0; i < mode2_it->second.size(); ++i) {
                const auto& net_rc = mode2_it->second[i];
                auto* net = net_rc.get();
                if (net == nullptr) {
                    throw std::logic_error(std::format("compute_bounding_boxes(): nullptr net in mode 2 at index {}", i));
                }
                if (net->modes().size() > 1) { continue; }
                auto bb = net->compute_bounding_box(2);
                if (bb.has_value()) {
                    boxes2.emplace_back(bb.value());
                } else {
                    no_bbox_mode2.emplace_back(net);
                }
            }

            return std::make_tuple(std::move(boxes1), std::move(boxes2), std::move(no_bbox_mode1), std::move(no_bbox_mode2));
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format("compute_bounding_boxes() failed: {}", err.what()));
        }
    }

    auto overlap(const circuit::Region& a, const circuit::Region& b) -> std::Option<circuit::Region> {
        auto r = circuit::Region{};
        r.row_min = std::max(a.row_min, b.row_min);
        r.row_max = std::min(a.row_max, b.row_max);
        r.col_min = std::max(a.col_min, b.col_min);
        r.col_max = std::min(a.col_max, b.col_max);
        if (r.row_min > r.row_max || r.col_min > r.col_max) {
            return std::nullopt;
        }
        return r;
    }

    auto compute_overlap_weights(
        const std::Vector<circuit::BoundingBox>& boxes1,
        const std::Vector<circuit::BoundingBox>& boxes2,
        const std::usize n
    ) -> std::Vector<std::Vector<std::i64>> {
        debug::info_fmt("computing overlap weights");

        auto weights = std::Vector<std::Vector<std::i64>>{};
        weights.resize(n);
        for (std::usize i = 0; i < n; ++i) {
            weights[i] = std::Vector<std::i64>(n, 0);
        }

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

        return weights;
    }

    auto match_nets(
        std::Vector<circuit::BoundingBox>& boxes1,
        std::Vector<circuit::BoundingBox>& boxes2,
        const std::Vector<std::usize>& assign,
        const std::Vector<std::Vector<std::i64>>& weights
    ) -> std::tuple<std::Vector<PairedNetGroup>, std::Vector<circuit::Net*>, std::Vector<circuit::Net*>> {
        std::usize paired = 0;
        std::i64 paired_weight_sum = 0;
        std::usize matched_to_dummy = 0;
        std::usize matched_no_overlap = 0;
        auto paired_net_names = std::String{};
        auto matched_to_dummy_names = std::String{};
        auto matched_no_overlap_names = std::String{};

        auto pairs = std::Vector<PairedNetGroup>{};
        auto unpaired_mode1 = std::Vector<circuit::Net*>{};
        auto unpaired_mode2 = std::Vector<circuit::Net*>{};

        for (std::usize i = 0; i < boxes1.size(); ++i) {
            const auto j = assign[i];
            if (j >= boxes2.size()) {
                ++matched_to_dummy;
                unpaired_mode1.emplace_back(boxes1[i].net);
                matched_to_dummy_names += boxes1[i].net->name() + " " + "\n";
                continue; // matched to dummy
            }
            auto ov = overlap(boxes1[i].region, boxes2[j].region);
            if (!ov.has_value()) {
                ++matched_no_overlap;
                unpaired_mode1.emplace_back(boxes1[i].net);
                unpaired_mode2.emplace_back(boxes2[j].net);
                matched_no_overlap_names += boxes1[i].net->name() + " " + 
                    " <-> " + boxes2[j].net->name() + " " + 
                    " overlap=(none)\n";
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
            paired_net_names += boxes1[i].net->name() + " " + 
                " <-> " + boxes2[j].net->name() + " " + 
                " overlap=" + "\n";
        }

        if (paired_net_names.empty()) { paired_net_names = "(none)"; }
        if (matched_to_dummy_names.empty()) { matched_to_dummy_names = "(none)"; }
        if (matched_no_overlap_names.empty()) { matched_no_overlap_names = "(none)"; }

        debug::info_fmt(
            "hungarian matching done: mode1_boxes={}, mode2_boxes={}, paired={}, matched_to_dummy={}, matched_no_overlap={}",
            boxes1.size(), boxes2.size(), paired, matched_to_dummy, matched_no_overlap
        );
        debug::info_fmt("hungarian paired net names:\n{}", paired_net_names);
        debug::info_fmt("hungarian matched_to_dummy net names:\n{}", matched_to_dummy_names);
        debug::info_fmt("hungarian matched_no_overlap net pairs:\n{}", matched_no_overlap_names);

        return std::make_tuple(std::move(pairs), std::move(unpaired_mode1), std::move(unpaired_mode2));
    }

    auto route_paired(
        hardware::Interposer* interposer,
        OccupancyView& view,
        HardwareRecorder& recorder,
        const std::Vector<PairedNetGroup>& pairs,
        const MultiModeParams& params
    ) -> std::tuple<std::usize, std::Vector<circuit::Net*>> {
        try {
            auto failed = std::Vector<circuit::Net*>{};
            std::usize paired_success = 0;

            for (const auto& p : pairs) {
                try {
                    if (p.net1 == nullptr || p.net2 == nullptr) {
                        throw std::logic_error("route_paired(): null net in paired group");
                    }

                    auto res = kiwi::algo::route_paired_nets_iterative(
                        interposer, view, recorder,
                        p.net1, 1,
                        p.net2, 2,
                        p.overlap, params
                    );
                    if (!res.success) {
                        debug::warning_fmt(
                            "route_paired(): failed pair net1='{}', net2='{}', overlap={}, k_candidates={}, converge_threshold={}",
                            p.net1->name(),
                            p.net2->name(),
                            format_region(p.overlap),
                            params.k_candidates,
                            params.converge_threshold
                        );
                        failed.emplace_back(p.net1);
                        failed.emplace_back(p.net2);
                        continue;
                    }

                    // Commit to hardware/pathpackage in main thread.
                    commit_paired_net_result(interposer, view, recorder, 1, p.net1, res.net1_result);
                    commit_paired_net_result(interposer, view, recorder, 2, p.net2, res.net2_result);
                    paired_success += 1;
                }
                catch (const std::exception& err) {
                    throw std::runtime_error(std::format(
                        "route_paired(): failed while processing pair net1='{}', net2='{}', overlap={}, reason={}",
                        p.net1 == nullptr ? std::String{"<null>"} : p.net1->name(),
                        p.net2 == nullptr ? std::String{"<null>"} : p.net2->name(),
                        format_region(p.overlap),
                        err.what()
                    ));
                }
            }
            debug::info_fmt("paired routing done: success={}, failed_pairs={}", paired_success, pairs.size() - paired_success);

            return std::make_tuple(paired_success, failed);
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format("route_paired() failed: {}", err.what()));
        }
    }

    auto collect_remaining_nets(
        const std::Vector<circuit::Net*>& no_bbox,
        const std::Vector<circuit::Net*>& unpaired
    ) -> std::Vector<circuit::Net*> {
        auto remaining = std::Vector<circuit::Net*>{};
        remaining.insert(remaining.end(), no_bbox.begin(), no_bbox.end());
        remaining.insert(remaining.end(), unpaired.begin(), unpaired.end());

        std::sort(remaining.begin(), remaining.end());
        remaining.erase(std::unique(remaining.begin(), remaining.end()), remaining.end());
        
        return remaining;
    }

    auto route_remaining_nets(
        hardware::Interposer* interposer,
        OccupancyView& view,
        HardwareRecorder& recorder,
        int mode,
        const std::Vector<circuit::Net*>& remaining,
        const algo::MazeRouteStrategy& maze
    ) -> void {
        try {
            std::Vector<circuit::Net*> routed_nets = {};
            for (auto* net : remaining) {
                try {
                    if (net == nullptr) {
                        throw std::logic_error(std::format("route_remaining_nets(): nullptr net in mode {}", mode));
                    }
                    if (net->modes().size() > 1) { continue; }

                    const auto net_modes = net->modes();
                    if (std::find(net_modes.begin(), net_modes.end(), mode) == net_modes.end()) {
                        continue;
                    }
                    
                    net->set_reuse_type(false);
                    net->search_related_nets(routed_nets);
                    net->route(interposer, maze);
                    routed_nets.emplace_back(net);

                    net->reset_pathpackage();
                    auto& pkg = net->pathpackage();
                    for (auto& [t, cob] : pkg._regular_path) {
                        (void)t;
                        if (cob.has_value()) { view.occupy_cobconnector(mode, cob.value(), false); }
                    }
                    for (auto& [b, tob, t] : pkg._tob_to_track) { (void)b; (void)t; view.occupy_tobconnector(mode, tob); }
                    for (auto& [b, tob, t] : pkg._track_to_tob) { (void)b; (void)t; view.occupy_tobconnector(mode, tob); }
                    recorder.update_recorders_history(pkg, false);
                }
                catch (const std::exception& err) {
                    throw std::runtime_error(std::format(
                        "route_remaining_nets(): mode={}, net='{}', reason={}",
                        mode,
                        net == nullptr ? std::String{"<null>"} : net->name(),
                        err.what()
                    ));
                }
            }
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format(
                "route_remaining_nets() failed: mode={}, remaining_count={}, reason={}",
                mode,
                remaining.size(),
                err.what()
            ));
        }
    }

}

