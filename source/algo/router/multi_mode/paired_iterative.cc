#include "./paired_iterative.hh"

#include <algo/router/common/maze/mazererouter.hh>
#include <algo/router/common/thread_log_buffer.hh>
#include <algo/router/routeerror.hh>
#include <algo/router/single_mode/incremental/recorders/hardware_recorder.hh>
#include <circuit/net/types/bbnet.hh>
#include <circuit/net/types/btnet.hh>
#include <circuit/net/types/tbnet.hh>
#include <circuit/net/types/syncnet.hh>
#include <circuit/path/pathpackage.hh>
#include <hardware/interposer.hh>
#include <debug/debug.hh>
#include <atomic>
#include <thread>
#include <cmath>
#include <stdexcept>
#include <std/format.hh>

namespace kiwi::algo {

    using ConnectorInfoTuple = std::Tuple<hardware::BumpCoord, circuit::TOBConnectorInfo, hardware::TrackCoord>;

    static auto finalize_three_segment_path(
        const std::Option<ConnectorInfoTuple>& tob_to_track_info,
        const std::Option<ConnectorInfoTuple>& track_to_tob_info,
        DeferredRegularPath seg1,
        DeferredRegularPath seg2,
        DeferredRegularPath seg3,
        bool has_begin_bump,
        bool has_end_bump
    ) -> circuit::ThreeSegmentDeferredPath {
        auto res = circuit::ThreeSegmentDeferredPath{};
        res._tob_to_track = tob_to_track_info;
        res._track_to_tob = track_to_tob_info;
        res._seg1 = std::move(seg1);
        res._seg2 = std::move(seg2);
        res._seg3 = std::move(seg3);

        // Conservative length: merged regular tracks + optional bump endpoints.
        const auto merged12 = circuit::ThreeSegmentDeferredPath::splice_regular(res._seg1, res._seg2);
        const auto merged123 = circuit::ThreeSegmentDeferredPath::splice_regular(merged12, res._seg3);
        res._length = merged123.size();
        if (has_begin_bump) { res._length += 1; }
        if (has_end_bump) { res._length += 1; }

        return res;
    }

    static auto build_three_segment_for_btb_net(
        hardware::Interposer* interposer,
        const OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode,
        const circuit::BumpToBumpNet* net,
        const hardware::COBCoord& entry,
        const hardware::COBCoord& exit,
        std::HashSet<hardware::Track*>& occupied_tracks
    ) -> std::Option<circuit::ThreeSegmentDeferredPath> {
        auto begin_bump = net->begin_bump();
        auto end_bump = net->end_bump();

        auto begin_tracks = std::Vector<hardware::Track*>{};
        auto end_tracks = std::HashSet<hardware::Track*>{};

        auto tob_to_track_info = std::Option<ConnectorInfoTuple>{std::nullopt};
        auto track_to_tob_info = std::Option<ConnectorInfoTuple>{std::nullopt};

        auto begin_map = view.available_tracks_bump_to_track(begin_bump, mode);
        for (auto& [t, _] : begin_map) {
            if (!occupied_tracks.contains(t)) {
                begin_tracks.emplace_back(t);
            }
        }
        if (begin_tracks.empty()) {
            return std::nullopt;
        }

        auto end_map = view.available_tracks_track_to_bump(end_bump, mode);
        for (auto& [t, _] : end_map) {
            if (!occupied_tracks.contains(t)) {
                end_tracks.emplace(t);
            }
        }
        if (end_tracks.empty()) {
            return std::nullopt;
        }

        // route first segment
        auto seg1 = maze_search_to_cob(view, recorder, reuse_type, mode, begin_tracks, entry, occupied_tracks);
        if (seg1.empty()) { return std::nullopt; }
        auto last1_coord = std::get<0>(seg1.back());
        auto last1 = interposer->get_track(last1_coord).value();

        // segment1 first track to begin bump
        auto first_track1_coord = std::get<0>(seg1.front());
        auto first_track1 = interposer->get_track(first_track1_coord).value();
        if (begin_map.find(first_track1) != begin_map.end()) {
            const auto& c = begin_map.find(first_track1)->second;
            tob_to_track_info.emplace(std::Tuple{
                begin_bump->coord(),
                circuit::TOBConnectorInfo{
                    c.bump_index(), c.hori_index(), c.vert_index(), c.track_index(),
                    c.single_direction(), begin_bump->tob()->coord()
                },
                first_track1->coord()
            });
        }
        else {
            throw std::runtime_error(std::format(
                "build_three_segment_for_btb_net(): first_track1 not found in begin_map, first_track1_coord: ({}, {}, dir={}, index={}), begin_map_size={}",
                first_track1_coord.row, first_track1_coord.col, static_cast<int>(first_track1_coord.dir), first_track1_coord.index, begin_map.size()
            ));
        }

        // route second segment
        auto seg2 = maze_search_to_cob(view, recorder, reuse_type, mode, std::Vector<hardware::Track*>{last1}, exit, occupied_tracks);
        if (seg2.empty()) { return std::nullopt; }
        auto last2_coord = std::get<0>(seg2.back());
        auto last2 = interposer->get_track(last2_coord).value();

        // route third segment
        auto seg3 = maze_search_to_tracks(view, recorder, reuse_type, mode, std::Vector<hardware::Track*>{last2}, end_tracks, occupied_tracks);
        if (seg3.empty()) { return std::nullopt; }

        // set segment3 last track to end bump
        auto last3_coord = std::get<0>(seg3.back());
        auto last3 = interposer->get_track(last3_coord).value();
        if (end_map.find(last3) != end_map.end()) {
            const auto& c = end_map.find(last3)->second;
            track_to_tob_info.emplace(std::Tuple{
                end_bump->coord(),
                circuit::TOBConnectorInfo{
                    c.bump_index(), c.hori_index(), c.vert_index(), c.track_index(),
                    c.single_direction(), end_bump->tob()->coord()
                },
                last3->coord()
            });
        }
        else {
            throw std::runtime_error(std::format(
                "build_three_segment_for_btb_net(): last3 not found in end_map, last3_coord: ({}, {}, dir={}, index={}), end_map_size={}",
                last3_coord.row, last3_coord.col, static_cast<int>(last3_coord.dir), last3_coord.index, end_map.size()
            ));
        }

        return finalize_three_segment_path(
            tob_to_track_info,
            track_to_tob_info,
            std::move(seg1),
            std::move(seg2),
            std::move(seg3),
            true,
            true
        );
    }

    static auto build_three_segment_for_ttb_net(
        hardware::Interposer* interposer,
        const OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode,
        const circuit::TrackToBumpNet* net,
        const hardware::COBCoord& entry,
        const hardware::COBCoord& exit,
        std::HashSet<hardware::Track*>& occupied_tracks
    ) -> std::Option<circuit::ThreeSegmentDeferredPath> {
        auto begin_track = net->begin_track();
        auto end_bump = net->end_bump();

        auto begin_tracks = std::Vector<hardware::Track*>{begin_track};
        auto end_tracks = std::HashSet<hardware::Track*>{};

        auto track_to_tob_info = std::Option<ConnectorInfoTuple>{std::nullopt};

        auto end_map = view.available_tracks_track_to_bump(end_bump, mode);
        for (auto& [t, _] : end_map) {
            if (!occupied_tracks.contains(t)) {
                end_tracks.emplace(t);
            }
        }
        if (end_tracks.empty()) {
            return std::nullopt;
        }

        auto seg1 = maze_search_to_cob(view, recorder, reuse_type, mode, begin_tracks, entry, occupied_tracks);
        if (seg1.empty()) { return std::nullopt; }
        auto last1_coord = std::get<0>(seg1.back());
        auto last1 = interposer->get_track(last1_coord).value();

        auto seg2 = maze_search_to_cob(view, recorder, reuse_type, mode, std::Vector<hardware::Track*>{last1}, exit, occupied_tracks);
        if (seg2.empty()) { return std::nullopt; }
        auto last2_coord = std::get<0>(seg2.back());
        auto last2 = interposer->get_track(last2_coord).value();

        auto seg3 = maze_search_to_tracks(view, recorder, reuse_type, mode, std::Vector<hardware::Track*>{last2}, end_tracks, occupied_tracks);
        if (seg3.empty()) { return std::nullopt; }

        // set segment3 last track to end bump
        auto last3_coord = std::get<0>(seg3.back());
        auto last3 = interposer->get_track(last3_coord).value();
        if (end_map.find(last3) != end_map.end()) {
            const auto& c = end_map.find(last3)->second;
            track_to_tob_info.emplace(std::Tuple{
                end_bump->coord(),
                circuit::TOBConnectorInfo{
                    c.bump_index(), c.hori_index(), c.vert_index(), c.track_index(),
                    c.single_direction(), end_bump->tob()->coord()
                },
                last3->coord()
            });
        }
        else {
            throw std::runtime_error(std::format(
                "build_three_segment_for_ttb_net(): last3 not found in end_map, last3_coord: ({}, {}, dir={}, index={}), end_map_size={}",
                last3_coord.row, last3_coord.col, static_cast<int>(last3_coord.dir), last3_coord.index, end_map.size()
            ));
        }

        return finalize_three_segment_path(
            std::nullopt,
            track_to_tob_info,
            std::move(seg1),
            std::move(seg2),
            std::move(seg3),
            false,
            true
        );
    }

    static auto build_three_segment_for_btt_net(
        hardware::Interposer* interposer,
        const OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode,
        const circuit::BumpToTrackNet* net,
        const hardware::COBCoord& entry,
        const hardware::COBCoord& exit,
        std::HashSet<hardware::Track*>& occupied_tracks
    ) -> std::Option<circuit::ThreeSegmentDeferredPath> {
        auto begin_bump = net->begin_bump();
        auto end_track = net->end_track();

        auto begin_tracks = std::Vector<hardware::Track*>{};
        auto end_tracks = std::HashSet<hardware::Track*>{end_track};

        auto tob_to_track_info = std::Option<ConnectorInfoTuple>{std::nullopt};

        auto begin_map = view.available_tracks_bump_to_track(begin_bump, mode);
        for (auto& [t, _] : begin_map) {
            if (!occupied_tracks.contains(t)) {
                begin_tracks.emplace_back(t);
            }
        }
        if (begin_tracks.empty()) {
            return std::nullopt;
        }

        auto seg1 = maze_search_to_cob(view, recorder, reuse_type, mode, begin_tracks, entry, occupied_tracks);
        if (seg1.empty()) { return std::nullopt; }
        auto last1_coord = std::get<0>(seg1.back());
        auto last1 = interposer->get_track(last1_coord).value();

        // segment1 first track to begin bump
        auto first_track1_coord = std::get<0>(seg1.front());
        auto first_track1 = interposer->get_track(first_track1_coord).value();
        if (begin_map.find(first_track1) != begin_map.end()) {
            const auto& c = begin_map.find(first_track1)->second;
            tob_to_track_info.emplace(std::Tuple{
                begin_bump->coord(),
                circuit::TOBConnectorInfo{
                    c.bump_index(), c.hori_index(), c.vert_index(), c.track_index(),
                    c.single_direction(), begin_bump->tob()->coord()
                },
                first_track1->coord()
            });
        }
        else {
            throw std::runtime_error(std::format(
                "build_three_segment_for_btt_net(): first_track1 not found in begin_map, first_track1_coord: ({}, {}, dir={}, index={}), begin_map_size={}",
                first_track1_coord.row, first_track1_coord.col, static_cast<int>(first_track1_coord.dir), first_track1_coord.index, begin_map.size()
            ));
        }

        auto seg2 = maze_search_to_cob(view, recorder, reuse_type, mode, std::Vector<hardware::Track*>{last1}, exit, occupied_tracks);
        if (seg2.empty()) { return std::nullopt; }
        auto last2_coord = std::get<0>(seg2.back());
        auto last2 = interposer->get_track(last2_coord).value();

        auto seg3 = maze_search_to_tracks(view, recorder, reuse_type, mode, std::Vector<hardware::Track*>{last2}, end_tracks, occupied_tracks);
        if (seg3.empty()) { return std::nullopt; }

        return finalize_three_segment_path(
            tob_to_track_info,
            std::nullopt,
            std::move(seg1),
            std::move(seg2),
            std::move(seg3),
            true,
            false
        );
    }

    static auto build_three_segment_for_syncnet(
        hardware::Interposer* interposer,
        const OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode,
        circuit::SyncNet* net,
        const hardware::COBCoord& entry,
        const hardware::COBCoord& exit,
        std::HashSet<hardware::Track*>& occupied_tracks
    ) -> std::Option<NetRouteHistoryResult> {
        struct SyncSubPath {
            circuit::PathPackage _pkg;
        };

        auto local_view = view;
        auto occupied_tracks_local = occupied_tracks;
        for (auto& sub_net : net->bttnets()) {
            occupied_tracks_local.emplace(sub_net->end_track());
        }
        for (auto& sub_net : net->ttbnets()) {
            occupied_tracks_local.emplace(sub_net->begin_track());
        }

        auto occupy_subnet_on_view = [&](const circuit::PathPackage& pkg) {
            for (const auto& [track, cob] : pkg._regular_path) {
                (void)track;
                if (cob.has_value()) {
                    local_view.occupy_cobconnector(mode, cob.value(), false);
                }
            }
            for (const auto& [bump, tob, track] : pkg._tob_to_track) {
                (void)bump;
                (void)track;
                local_view.occupy_tobconnector(mode, tob);
            }
            for (const auto& [bump, tob, track] : pkg._track_to_tob) {
                (void)bump;
                (void)track;
                local_view.occupy_tobconnector(mode, tob);
            }
        };

        auto paths = std::Vector<SyncSubPath>{};
        paths.reserve(net->btbnets().size() + net->ttbnets().size() + net->bttnets().size());
        std::usize max_length{0};

        for (auto& sub_net : net->btbnets()) {
            auto deferred = build_three_segment_for_btb_net(
                interposer, local_view, recorder, reuse_type, mode, sub_net.get(), entry, exit, occupied_tracks_local
            );
            if (!deferred.has_value()) {
                return std::nullopt;
            }
            auto history = deferred.value().to_history_pathpackage();
            auto pkg = circuit::PathPackage{history, interposer};
            max_length = std::max(max_length, pkg._length);
            occupy_subnet_on_view(pkg);
            paths.emplace_back(SyncSubPath{std::move(pkg)});
        }
        for (auto& sub_net : net->ttbnets()) {
            auto deferred = build_three_segment_for_ttb_net(
                interposer, local_view, recorder, reuse_type, mode, sub_net.get(), entry, exit, occupied_tracks_local
            );
            if (!deferred.has_value()) {
                return std::nullopt;
            }
            auto history = deferred.value().to_history_pathpackage();
            auto pkg = circuit::PathPackage{history, interposer};
            max_length = std::max(max_length, pkg._length);
            occupy_subnet_on_view(pkg);
            paths.emplace_back(SyncSubPath{std::move(pkg)});
        }
        for (auto& sub_net : net->bttnets()) {
            auto deferred = build_three_segment_for_btt_net(
                interposer, local_view, recorder, reuse_type, mode, sub_net.get(), entry, exit, occupied_tracks_local
            );
            if (!deferred.has_value()) {
                return std::nullopt;
            }
            auto history = deferred.value().to_history_pathpackage();
            auto pkg = circuit::PathPackage{history, interposer};
            max_length = std::max(max_length, pkg._length);
            occupy_subnet_on_view(pkg);
            paths.emplace_back(SyncSubPath{std::move(pkg)});
        }

        if (paths.empty()) {
            return std::nullopt;
        }

        auto rerouter = MazeRerouter{true};
        rerouter.set_recorder(&recorder);
        for (std::usize iter = 0; iter < 100; ++iter) {
            bool has_shorter_path = false;
            bool success_all = true;

            for (auto& sub_path : paths) {
                if (sub_path._pkg._length >= max_length) {
                    continue;
                }
                has_shorter_path = true;

                auto [success, ml] = rerouter.bus_reroute(interposer, &sub_path._pkg, max_length, reuse_type);
                max_length = std::max(max_length, ml);
                if (!success) {
                    success_all = false;
                    break;
                }
            }

            if (!has_shorter_path) {
                break;
            }
            if (!success_all) {
                continue;
            }
        }
        for (const auto& sub_path : paths) {
            if (sub_path._pkg._length < max_length) {
                return std::nullopt;
            }
        }

        // occupied_tracks_local is only used for endpoint-track protection inside sync routing.
        occupied_tracks = std::move(occupied_tracks_local);

        auto result = NetRouteHistoryResult{};
        auto aggregate_pkg = circuit::PathPackage{};
        result.subnet_histories.reserve(paths.size());
        for (auto& sub_path : paths) {
            auto hist = circuit::HistoryPathPackage{sub_path._pkg};
            if (hist._regular_path.empty()) {
                return std::nullopt;
            }
            result.subnet_histories.emplace_back(hist);
            aggregate_pkg._regular_path.insert(
                aggregate_pkg._regular_path.end(),
                sub_path._pkg._regular_path.begin(),
                sub_path._pkg._regular_path.end()
            );
            aggregate_pkg._tob_to_track.insert(
                aggregate_pkg._tob_to_track.end(),
                sub_path._pkg._tob_to_track.begin(),
                sub_path._pkg._tob_to_track.end()
            );
            aggregate_pkg._track_to_tob.insert(
                aggregate_pkg._track_to_tob.end(),
                sub_path._pkg._track_to_tob.begin(),
                sub_path._pkg._track_to_tob.end()
            );
            aggregate_pkg._length += sub_path._pkg._length;
        }
        result.aggregate_history = circuit::HistoryPathPackage{aggregate_pkg};
        return result;
    }

    static auto build_route_history_for_regular_net(
        hardware::Interposer* interposer,
        const OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode,
        circuit::Net* net,
        const hardware::COBCoord& entry,
        const hardware::COBCoord& exit,
        std::HashSet<hardware::Track*>& occupied_tracks
    ) -> std::Option<NetRouteHistoryResult> {
        auto deferred = std::Option<circuit::ThreeSegmentDeferredPath>{std::nullopt};
        if (auto* btb = dynamic_cast<circuit::BumpToBumpNet*>(net)) {
            log_info_fmt("build_three_segment_for_net(): btb net='{}', mode={}", net->name(), mode);
            deferred = build_three_segment_for_btb_net(interposer, view, recorder, reuse_type, mode, btb, entry, exit, occupied_tracks);
        } else if (auto* ttb = dynamic_cast<circuit::TrackToBumpNet*>(net)) {
            log_info_fmt("build_three_segment_for_net(): ttb net='{}', mode={}", net->name(), mode);
            deferred = build_three_segment_for_ttb_net(interposer, view, recorder, reuse_type, mode, ttb, entry, exit, occupied_tracks);
        } else if (auto* btt = dynamic_cast<circuit::BumpToTrackNet*>(net)) {
            log_info_fmt("build_three_segment_for_net(): btt net='{}', mode={}", net->name(), mode);
            deferred = build_three_segment_for_btt_net(interposer, view, recorder, reuse_type, mode, btt, entry, exit, occupied_tracks);
        } else {
            return std::nullopt;
        }
        if (!deferred.has_value()) {
            return std::nullopt;
        }

        auto history = deferred.value().to_history_pathpackage();
        auto result = NetRouteHistoryResult{};
        result.subnet_histories.emplace_back(history);
        result.aggregate_history = history;
        return result;
    }

    static auto build_three_segment_for_net(
        hardware::Interposer* interposer,
        const OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode,
        circuit::Net* net,
        const hardware::COBCoord& entry,
        const hardware::COBCoord& exit,
        std::HashSet<hardware::Track*>& occupied_tracks
    ) -> std::Option<NetRouteHistoryResult> {
        if (auto* sync = dynamic_cast<circuit::SyncNet*>(net)) {
            log_info_fmt("build_three_segment_for_net(): sync net='{}', mode={}", net->name(), mode);
            return build_three_segment_for_syncnet(interposer, view, recorder, reuse_type, mode, sync, entry, exit, occupied_tracks);
        }
        return build_route_history_for_regular_net(
            interposer, view, recorder, reuse_type, mode, net, entry, exit, occupied_tracks
        );
    }

    static auto recorder_total_cost(const algo::HardwareRecorder& recorder) -> double {
        const auto [cost_reuse, cost_nonreuse] = recorder.show_cost();
        return static_cast<double>(cost_reuse) + static_cast<double>(cost_nonreuse);
    }

    auto route_paired_nets_iterative(
        hardware::Interposer* interposer,
        const OccupancyView& base_view,
        const algo::HardwareRecorder& base_recorder,
        circuit::Net* net1, int mode1,
        circuit::Net* net2, int mode2,
        const circuit::Region& overlap_region,
        const MultiModeParams& params
    ) -> PairedRouteResult {
log_info_fmt("route_paired_nets_iterative(): net1='{}', net2='{}', mode1={}, mode2={}", net1->name(), net2->name(), mode1, mode2);
        if (net1 == nullptr || net2 == nullptr) {
            throw std::runtime_error("route_paired_nets_iterative(): net1 or net2 is nullptr");
        }
        if (mode1 <= 0 || mode2 <= 0) {
            throw std::runtime_error(std::format(
                "route_paired_nets_iterative(): invalid mode values mode1={}, mode2={}",
                mode1,
                mode2
            ));
        }

        auto cands = std::Vector<CobPairCandidate>{};
        try {
            cands = select_entry_exit_candidates(net1, net2, overlap_region, params.k_candidates);
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format(
                "route_paired_nets_iterative(): candidate selection failed for net1='{}', net2='{}', mode1={}, mode2={}, reason={}",
                net1->name(),
                net2->name(),
                mode1,
                mode2,
                err.what()
            ));
        }
        if (cands.empty()) {
            log_warning_fmt(
                "route_paired_nets_iterative(): no candidates for net1='{}', net2='{}', mode1={}, mode2={}",
                net1->name(),
                net2->name(),
                mode1,
                mode2
            );
            return {};
        }

        std::atomic<bool> stop{false};
        std::atomic<int> winner{-1};
        auto results = std::Vector<PairedRouteResult>(cands.size());
        auto candidate_logs = std::Vector<std::String>(cands.size(), std::String{});

        auto worker = [&](std::usize idx) {
            auto log_buffer = std::String{};
            auto log_scope = ScopedThreadLogBuffer{log_buffer};
            try {
                if (stop.load()) { return; }
                auto local_base_view = base_view; // copy
                auto local_recorder = base_recorder; // copy
                auto& cand = cands[idx];
                std::HashSet<hardware::Track*> occupied_tracks {};

                const auto net1_first = (net1->priority().value() <= net2->priority().value());

                auto last_cost = std::Option<double>{std::nullopt};
                auto last_h1 = std::Option<circuit::HistoryPathPackage>{std::nullopt};
                auto last_h2 = std::Option<circuit::HistoryPathPackage>{std::nullopt};

                for (std::usize iter = 0; iter < 100; ++iter) {
                    log_info_fmt("route_paired_nets_iterative(): iter={}", iter);
                    if (stop.load()) { return; }

                    if (last_h1.has_value()) {
                        auto pkg = circuit::PathPackage{last_h1.value(), interposer};
                        local_recorder.clear_history_records(pkg, false);
                    }
                    if (last_h2.has_value()) {
                        auto pkg = circuit::PathPackage{last_h2.value(), interposer};
                        local_recorder.clear_history_records(pkg, false);
                    }

                    auto local_view = local_base_view;

                    auto p_first = net1_first
                        ? build_three_segment_for_net(interposer, local_view, local_recorder, false, mode1, net1, cand.entry, cand.exit, occupied_tracks)
                        : build_three_segment_for_net(interposer, local_view, local_recorder, false, mode2, net2, cand.entry, cand.exit, occupied_tracks);
                    if (!p_first.has_value() || stop.load()) { return; }

                    auto first_result = p_first.value();
                    {
                        auto pkg = circuit::PathPackage{first_result.aggregate_history, interposer};
                        local_recorder.update_recorders_current(pkg, false);
                    }

                    auto p_second = net1_first
                        ? build_three_segment_for_net(interposer, local_view, local_recorder, false, mode2, net2, cand.entry, cand.exit, occupied_tracks)
                        : build_three_segment_for_net(interposer, local_view, local_recorder, false, mode1, net1, cand.entry, cand.exit, occupied_tracks);
                    if (!p_second.has_value() || stop.load()) { return; }

                    auto second_result = p_second.value();
                    {
                        auto pkg = circuit::PathPackage{second_result.aggregate_history, interposer};
                        local_recorder.update_recorders_current(pkg, false);
                    }

                    {
                        auto pkg = circuit::PathPackage{first_result.aggregate_history, interposer};
                        local_recorder.update_recorders_history(pkg, false);
                    }
                    {
                        auto pkg = circuit::PathPackage{second_result.aggregate_history, interposer};
                        local_recorder.update_recorders_history(pkg, false);
                    }

                    const auto [_, cost_now] = local_recorder.show_cost();
                    if (last_cost.has_value() && std::fabs(cost_now - last_cost.value()) < params.converge_threshold) {
                        PairedRouteResult r{};
                        r.success = true;
                        r.used_cob_pair = cand;
                        if (net1_first) {
                            r.net1_result = std::move(first_result);
                            r.net2_result = std::move(second_result);
                        } else {
                            r.net1_result = std::move(second_result);
                            r.net2_result = std::move(first_result);
                        }
                        results[idx] = std::move(r);
                        break;
                    }

                    last_cost.emplace(cost_now);
                    if (net1_first) {
                        last_h1.emplace(first_result.aggregate_history);
                        last_h2.emplace(second_result.aggregate_history);
                    } else {
                        last_h1.emplace(second_result.aggregate_history);
                        last_h2.emplace(first_result.aggregate_history);
                    }
                }

                if (!results[idx].success) {
                    log_warning_fmt(
                        "route_paired_nets_iterative(): candidate {} did not converge for net1='{}', net2='{}', entry=({}, {}), exit=({}, {}), threshold={}",
                        idx,
                        net1->name(),
                        net2->name(),
                        cands[idx].entry.row,
                        cands[idx].entry.col,
                        cands[idx].exit.row,
                        cands[idx].exit.col,
                        params.converge_threshold
                    );
                    candidate_logs[idx] = log_buffer;
                    return;
                }

                int expected = -1;
                if (winner.compare_exchange_strong(expected, static_cast<int>(idx))) {
                    stop.store(true);
                }
                candidate_logs[idx] = log_buffer;
            }
            catch (const std::exception& err) {
                log_error_fmt(
                    "route_paired_nets_iterative(): worker failed, candidate={}, net1='{}', net2='{}', entry=({}, {}), exit=({}, {}), reason={}",
                    idx,
                    net1->name(),
                    net2->name(),
                    cands[idx].entry.row,
                    cands[idx].entry.col,
                    cands[idx].exit.row,
                    cands[idx].exit.col,
                    err.what()
                );
                candidate_logs[idx] = log_buffer;
                results[idx] = PairedRouteResult{};
            }
        };

        auto threads = std::Vector<std::thread>{};
        threads.reserve(cands.size());
        for (std::usize i = 0; i < cands.size(); ++i) {
            threads.emplace_back(worker, i);
        }
        for (auto& th : threads) {
            if (th.joinable()) { th.join(); }
        }
        for (std::usize i = 0; i < candidate_logs.size(); ++i) {
            if (candidate_logs[i].empty()) {
                continue;
            }
            log_info_fmt("===== paired worker candidate {} begin =====", i);
            debug::info(candidate_logs[i]);
            log_info_fmt("===== paired worker candidate {} end =====", i);
        }

        const auto w = winner.load();
        if (w < 0) {
            log_warning_fmt(
                "route_paired_nets_iterative(): all candidates failed for net1='{}', net2='{}', candidates={}, mode1={}, mode2={}",
                net1->name(),
                net2->name(),
                cands.size(),
                mode1,
                mode2
            );
            return {};
        }
        return results[static_cast<std::usize>(w)];
    }

}

