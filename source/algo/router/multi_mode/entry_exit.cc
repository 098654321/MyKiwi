#include "./entry_exit.hh"

#include <hardware/interposer.hh>
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace kiwi::algo {

    static auto manhattan(const hardware::Coord& a, const hardware::Coord& b) -> std::i64 {
        return std::llabs(a.row - b.row) + std::llabs(a.col - b.col);
    }

    static auto manhattan_to_cob(const hardware::Coord& a, const hardware::COBCoord& b) -> std::i64 {
        return std::llabs(a.row - b.row) + std::llabs(a.col - b.col);
    }

    static auto net_endpoints(const circuit::Net& net) -> std::Option<std::Pair<hardware::Coord, hardware::Coord>> {
        // currently only support 2-pin nets or syncnet, where syncnet should have only 2 different endpoints when treated as a whole net
        auto coords = net.coords();
        if (coords.size() < 2) {
            throw std::runtime_error(std::format("net {} has less than 2 coords", net.name()));
        }

        std::unordered_set<hardware::Coord> unique_coords;
        for (const auto& coord : coords) {
            unique_coords.emplace(coord);
        }
        if (unique_coords.size() != 2) {
            throw std::runtime_error(std::format("net {} has more than 2 unique coords", net.name()));
        }
        auto it = unique_coords.begin();
        return std::Pair<hardware::Coord, hardware::Coord>{*it, *(++it)};
    }

    auto select_entry_exit_candidates(
        const circuit::Net& net1,
        const circuit::Net& net2,
        const circuit::Region& overlap_region,
        std::usize k
    ) -> std::Vector<CobPairCandidate> {
        auto ep1 = net_endpoints(net1);
        auto ep2 = net_endpoints(net2);
        if (!ep1.has_value() || !ep2.has_value()) {
            throw std::runtime_error(std::format("net {} or net {} got empty endpoints", net1.name(), net2.name()));
        }

        const auto [s1, t1] = ep1.value();
        const auto [s2, t2] = ep2.value();

        const auto distance_net = manhattan(s1, t1) + manhattan(s2, t2);

        auto r = overlap_region;
        if (r.row_min > r.row_max || r.col_min > r.col_max) {
            throw std::runtime_error(std::format("overlap region is empty"));
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
                            2*cob_to_cob +
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
        if (candidates.empty()) {
            throw std::runtime_error(std::format("no entry exit candidates found for net {} and net {}", net1.name(), net2.name()));
        }
        
        return candidates;
    }

}

