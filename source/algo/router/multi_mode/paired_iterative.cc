#include "./paired_iterative.hh"

#include <algo/router/routeerror.hh>
#include <algo/router/single_mode/incremental/recorders/hardware_recorder.hh>
#include <circuit/net/types/bbnet.hh>
#include <circuit/net/types/btnet.hh>
#include <circuit/net/types/tbnet.hh>
#include <circuit/path/pathpackage.hh>
#include <hardware/interposer.hh>
#include <atomic>
#include <thread>

namespace kiwi::algo::multi_mode {

    static auto build_three_segment_for_simple_net(
        hardware::Interposer* interposer,
        const OccupancyView& view,
        int mode,
        const circuit::Net& net,
        const hardware::COBCoord& entry,
        const hardware::COBCoord& exit
    ) -> std::Option<circuit::ThreeSegmentDeferredPath> {
        // Currently supports 2-pin nets via coords() heuristic:
        // - If endpoint is bump, prefer TOB available tracks (shared=true) then filter by OccupancyView.
        // - If endpoint is track, use that track.

        auto coords = net.coords();
        if (coords.size() < 2) {
            return std::nullopt;
        }

        // Identify whether endpoints are bumps or tracks by dynamic_cast.
        const hardware::Bump* begin_bump = nullptr;
        const hardware::Bump* end_bump = nullptr;
        const hardware::Track* begin_track = nullptr;
        const hardware::Track* end_track = nullptr;

        if (auto* bb = dynamic_cast<const circuit::BumpToBumpNet*>(&net)) {
            begin_bump = bb->begin_bump();
            end_bump = bb->end_bump();
        } else if (auto* bt = dynamic_cast<const circuit::BumpToTrackNet*>(&net)) {
            begin_bump = bt->begin_bump();
            end_track = bt->end_track();
        } else if (auto* tb = dynamic_cast<const circuit::TrackToBumpNet*>(&net)) {
            begin_track = tb->begin_track();
            end_bump = tb->end_bump();
        } else {
            return std::nullopt;
        }

        auto begin_tracks = std::Vector<hardware::Track*>{};
        auto end_tracks = std::HashSet<hardware::Track*>{};

        auto tob_to_track_info = std::Option<std::Tuple<hardware::BumpCoord, circuit::TOBConnectorInfo, hardware::TrackCoord>>{std::nullopt};
        auto track_to_tob_info = std::Option<std::Tuple<hardware::BumpCoord, circuit::TOBConnectorInfo, hardware::TrackCoord>>{std::nullopt};

        if (begin_bump != nullptr) {
            auto map = interposer->available_tracks_bump_to_track(const_cast<hardware::Bump*>(begin_bump), true);
            for (auto& [t, _] : map) {
                if (view.is_idle_track(mode, t)) {
                    begin_tracks.emplace_back(t);
                }
            }
            if (begin_tracks.empty()) {
                return std::nullopt;
            }
            // pick the first begin track for connector info
            auto* head = begin_tracks.front();
            auto it = map.find(head);
            if (it != map.end()) {
                const auto& c = it->second;
                tob_to_track_info.emplace(std::Tuple{
                    begin_bump->coord(),
                    circuit::TOBConnectorInfo{
                        c.bump_index(), c.hori_index(), c.vert_index(), c.track_index(),
                        c.single_direction(), begin_bump->tob()->coord()
                    },
                    head->coord()
                });
            }
        } else if (begin_track != nullptr) {
            begin_tracks.emplace_back(const_cast<hardware::Track*>(begin_track));
        }

        if (end_bump != nullptr) {
            auto map = interposer->available_tracks_track_to_bump(const_cast<hardware::Bump*>(end_bump), true);
            for (auto& [t, _] : map) {
                if (view.is_idle_track(mode, t)) {
                    end_tracks.emplace(t);
                }
            }
            if (end_tracks.empty()) {
                return std::nullopt;
            }
            // pick an arbitrary end track for connector info
            auto* tail = *end_tracks.begin();
            auto it = map.find(tail);
            if (it != map.end()) {
                const auto& c = it->second;
                track_to_tob_info.emplace(std::Tuple{
                    end_bump->coord(),
                    circuit::TOBConnectorInfo{
                        c.bump_index(), c.hori_index(), c.vert_index(), c.track_index(),
                        c.single_direction(), end_bump->tob()->coord()
                    },
                    tail->coord()
                });
            }
        } else if (end_track != nullptr) {
            end_tracks.emplace(const_cast<hardware::Track*>(end_track));
        }

        auto occupied = std::HashSet<hardware::Track*>{};
        auto seg1 = maze_search_to_cob(view, mode, begin_tracks, entry, occupied);
        if (seg1.empty()) { return std::nullopt; }
        auto last1_coord = std::get<0>(seg1.back());
        auto last1 = interposer->get_track(last1_coord).value();

        auto seg2 = maze_search_to_cob(view, mode, std::Vector<hardware::Track*>{last1}, exit, occupied);
        if (seg2.empty()) { return std::nullopt; }
        auto last2_coord = std::get<0>(seg2.back());
        auto last2 = interposer->get_track(last2_coord).value();

        auto seg3 = maze_search_to_tracks(view, mode, std::Vector<hardware::Track*>{last2}, end_tracks, occupied);
        if (seg3.empty()) { return std::nullopt; }

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
        if (begin_bump != nullptr) { res._length += 1; }
        if (end_bump != nullptr) { res._length += 1; }

        return res;
    }

    static auto recorder_total_cost(const algo::HardwareRecorder& recorder) -> double {
        const auto [cost_reuse, cost_nonreuse] = recorder.show_cost();
        return static_cast<double>(cost_reuse) + static_cast<double>(cost_nonreuse);
    }

    auto route_paired_nets_iterative(
        hardware::Interposer* interposer,
        const OccupancyView& base_view,
        const algo::HardwareRecorder& base_recorder,
        const circuit::Net& net1, int mode1,
        const circuit::Net& net2, int mode2,
        const circuit::Region& overlap_region,
        const MultiModeParams& params
    ) -> PairedRouteResult {
        auto cands = select_entry_exit_candidates(net1, net2, overlap_region, params.k_candidates);
        if (cands.empty()) {
            return {};
        }

        std::atomic<bool> stop{false};
        std::atomic<int> winner{-1};
        auto results = std::Vector<PairedRouteResult>(cands.size());

        auto worker = [&](std::usize idx) {
            if (stop.load()) { return; }
            auto local_base_view = base_view; // copy
            auto local_recorder = base_recorder; // copy
            auto& cand = cands[idx];

            // Pick routing order by priority (lower value => higher priority).
            const auto net1_first = (net1.priority().value() <= net2.priority().value());

            auto last_cost = std::Option<double>{std::nullopt};
            auto last_h1 = std::Option<circuit::HistoryPathPackage>{std::nullopt};
            auto last_h2 = std::Option<circuit::HistoryPathPackage>{std::nullopt};

            // Iterate until convergence or failure.
            for (std::usize iter = 0; iter < 100; ++iter) {
                if (stop.load()) { return; }

                // Clear last iteration's history influence (best-effort, mirrors incremental iterate_routing()).
                if (last_h1.has_value()) {
                    auto pkg = circuit::PathPackage{last_h1.value(), interposer};
                    local_recorder.clear_history_records(pkg, false);
                }
                if (last_h2.has_value()) {
                    auto pkg = circuit::PathPackage{last_h2.value(), interposer};
                    local_recorder.clear_history_records(pkg, false);
                }

                auto local_view = local_base_view; // start from base occupancy each iteration

                auto p_first = net1_first
                    ? build_three_segment_for_simple_net(interposer, local_view, mode1, net1, cand.entry, cand.exit)
                    : build_three_segment_for_simple_net(interposer, local_view, mode2, net2, cand.entry, cand.exit);
                if (!p_first.has_value() || stop.load()) { return; }

                auto h_first = p_first.value().to_history_pathpackage();
                {
                    auto pkg = circuit::PathPackage{h_first, interposer};
                    local_recorder.update_recorders_current(pkg, false);
                }

                auto p_second = net1_first
                    ? build_three_segment_for_simple_net(interposer, local_view, mode2, net2, cand.entry, cand.exit)
                    : build_three_segment_for_simple_net(interposer, local_view, mode1, net1, cand.entry, cand.exit);
                if (!p_second.has_value() || stop.load()) { return; }

                auto h_second = p_second.value().to_history_pathpackage();
                {
                    auto pkg = circuit::PathPackage{h_second, interposer};
                    local_recorder.update_recorders_current(pkg, false);
                }

                // Update history for both.
                {
                    auto pkg = circuit::PathPackage{h_first, interposer};
                    local_recorder.update_recorders_history(pkg, false);
                }
                {
                    auto pkg = circuit::PathPackage{h_second, interposer};
                    local_recorder.update_recorders_history(pkg, false);
                }

                const auto cost_now = recorder_total_cost(local_recorder);
                if (last_cost.has_value() && std::fabs(cost_now - last_cost.value()) < params.converge_threshold) {
                    // Converged: assign results to (net1, net2) order.
                    PairedRouteResult r{};
                    r.success = true;
                    r.used_cob_pair = cand;
                    if (net1_first) {
                        r.net1_history = std::move(h_first);
                        r.net2_history = std::move(h_second);
                    } else {
                        r.net1_history = std::move(h_second);
                        r.net2_history = std::move(h_first);
                    }
                    results[idx] = std::move(r);
                    break;
                }

                last_cost.emplace(cost_now);
                if (net1_first) {
                    last_h1.emplace(h_first);
                    last_h2.emplace(h_second);
                } else {
                    last_h1.emplace(h_second);
                    last_h2.emplace(h_first);
                }
            }

            if (!results[idx].success) {
                return;
            }

            PairedRouteResult r{};
            int expected = -1;
            if (winner.compare_exchange_strong(expected, static_cast<int>(idx))) {
                stop.store(true);
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

        const auto w = winner.load();
        if (w < 0) {
            return {};
        }
        return results[static_cast<std::usize>(w)];
    }

}

