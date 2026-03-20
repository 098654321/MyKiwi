#include "./entry_exit.hh"

#include <hardware/interposer.hh>
#include <algorithm>
#include <stdexcept>

namespace kiwi::algo {

    static auto manhattan(const hardware::Coord& a, const hardware::Coord& b) -> std::i64 {
        return std::llabs(a.row - b.row) + std::llabs(a.col - b.col);
    }

    static auto manhattan_to_cob(const hardware::Coord& a, const hardware::COBCoord& b) -> std::i64 {
        return std::llabs(a.row - b.row) + std::llabs(a.col - b.col);
    }

    static auto net_endpoints_cost(const circuit::Net& net) -> std::Option<std::Pair<hardware::Coord, hardware::Coord>> {
        // For now, use the first/last coords as (start,end). For SyncNet, this will be refined later.
        auto coords = net.coords();
        if (coords.size() < 2) {
            return std::nullopt;
        }
        return std::Pair<hardware::Coord, hardware::Coord>{coords.front(), coords.back()};
    }

    auto select_entry_exit_candidates(
        const circuit::Net& net1,
        const circuit::Net& net2,
        const circuit::Region& overlap_region,
        std::usize k
    ) -> std::Vector<CobPairCandidate> {
        auto ep1 = net_endpoints_cost(net1);
        auto ep2 = net_endpoints_cost(net2);
        if (!ep1.has_value() || !ep2.has_value()) {
            return {};
        }

        const auto [s1, t1] = ep1.value();
        const auto [s2, t2] = ep2.value();

        const auto distance_net = manhattan(s1, t1) + manhattan(s2, t2);

        auto r = overlap_region;
        r.normalize();
        // Clamp to COB array range.
        r.row_min = std::max<std::i64>(0, r.row_min);
        r.col_min = std::max<std::i64>(0, r.col_min);
        r.row_max = std::min<std::i64>(hardware::Interposer::COB_ARRAY_HEIGHT - 1, r.row_max);
        r.col_max = std::min<std::i64>(hardware::Interposer::COB_ARRAY_WIDTH - 1, r.col_max);
        if (r.row_min > r.row_max || r.col_min > r.col_max) {
            return {};
        }

        auto candidates = std::Vector<CobPairCandidate>{};
        for (std::i64 er = r.row_min; er <= r.row_max; ++er) {
            for (std::i64 ec = r.col_min; ec <= r.col_max; ++ec) {
                const auto entry = hardware::COBCoord{er, ec};
                for (std::i64 xr = r.row_min; xr <= r.row_max; ++xr) {
                    for (std::i64 xc = r.col_min; xc <= r.col_max; ++xc) {
                        const auto exit = hardware::COBCoord{xr, xc};
                        const auto cob_to_cob = manhattan(hardware::Coord{entry.row, entry.col}, hardware::Coord{exit.row, exit.col});

        const auto distance_cob =
                            cob_to_cob +
                            manhattan_to_cob(s1, entry) + manhattan_to_cob(s2, entry) +
                            manhattan_to_cob(t1, exit) + manhattan_to_cob(t2, exit);

                        if (distance_cob != distance_net) {
                            continue;
                        }
                        candidates.emplace_back(CobPairCandidate{entry, exit, cob_to_cob});
                    }
                }
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            return a.cob_to_cob_dist > b.cob_to_cob_dist;
        });

        if (candidates.size() > k) {
            candidates.resize(k);
        }
        return candidates;
    }

}

