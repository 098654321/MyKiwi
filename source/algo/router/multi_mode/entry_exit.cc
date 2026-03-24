#include "./entry_exit.hh"
#include "circuit/net/types/bbnet.hh"
#include "circuit/net/types/btnet.hh"
#include "circuit/net/types/tbnet.hh"

#include <hardware/interposer.hh>
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace kiwi::algo {

    static auto format_region(const circuit::Region& region) -> std::String {
        return std::format(
            "rows=[{},{}], cols=[{},{}]",
            region.row_min,
            region.row_max,
            region.col_min,
            region.col_max
        );
    }

    static auto manhattan(const hardware::Coord& a, const hardware::Coord& b) -> std::i64 {
        return std::llabs(a.row - b.row) + std::llabs(a.col - b.col);
    }

    static auto manhattan_to_cob(const hardware::Coord& a, const hardware::COBCoord& b) -> std::i64 {
        return std::llabs(a.row - b.row) + std::llabs(a.col - b.col);
    }

    static auto net_endpoints(circuit::Net* net) -> std::Option<std::Pair<hardware::Coord, hardware::Coord>> {
        // currently only support 2-pin nets or syncnet, where syncnet should have only 2 different endpoints when treated as a whole net
        if (net == nullptr) {
            throw std::runtime_error("net_endpoints(): net is nullptr");
        }

        return std::Pair<hardware::Coord, hardware::Coord>{net->net_begin_cob(), net->net_end_cob()};
    }

    auto select_entry_exit_candidates(
        circuit::Net* net1,
        circuit::Net* net2,
        const circuit::Region& overlap_region,
        std::usize k
    ) -> std::Vector<CobPairCandidate> {
        try {
            if (net1 == nullptr || net2 == nullptr) {
                throw std::runtime_error("select_entry_exit_candidates(): net1 or net2 is nullptr");
            }

            auto ep1 = net_endpoints(net1);
            auto ep2 = net_endpoints(net2);
            if (!ep1.has_value() || !ep2.has_value()) {
                throw std::runtime_error(std::format(
                    "select_entry_exit_candidates(): net1='{}' or net2='{}' has empty endpoints",
                    net1->name(),
                    net2->name()
                ));
            }

            // s/t here is the cob coordinates of the endpoints of the net, not including the bump_length
            const auto [s1, t1] = ep1.value();
            const auto [s2, t2] = ep2.value();

            // calculate net distance
            const auto distance_net = std::min(
                net1->manhattan_to_net_begin_point(t1), net1->manhattan_to_net_end_point(s1)
            ) + std::min(
                net2->manhattan_to_net_begin_point(t2), net2->manhattan_to_net_end_point(s2)
            ) + net1->port_length() + net2->port_length();

            auto r = overlap_region;
            if (r.row_min > r.row_max || r.col_min > r.col_max) {
                throw std::runtime_error(std::format(
                    "select_entry_exit_candidates(): overlap region is empty for net1='{}', net2='{}', overlap={}",
                    net1->name(),
                    net2->name(),
                    format_region(r)
                ));
            }

            auto candidates = std::Vector<CobPairCandidate>{};
            std::usize checked_pairs = 0;
            for (std::i64 er = r.row_min; er <= r.row_max; ++er) {
                for (std::i64 ec = r.col_min; ec <= r.col_max; ++ec) {
                    const auto entry = hardware::COBCoord{er, ec};
                    for (std::i64 xr = r.row_min; xr <= r.row_max; ++xr) {
                        for (std::i64 xc = r.col_min; xc <= r.col_max; ++xc) {
                            const auto exit = hardware::COBCoord{xr, xc};
                            checked_pairs += 1;

                            const auto cob_to_cob_distance = net1->manhattan_cob_to_cob(entry, exit) + net2->manhattan_cob_to_cob(entry, exit);
                            const auto distance_cob =
                                cob_to_cob_distance +
                                net1->manhattan_to_net_begin_point(entry) + net2->manhattan_to_net_begin_point(entry) +
                                net1->manhattan_to_net_end_point(exit) + net2->manhattan_to_net_end_point(exit) + 
                                net1->port_length() + net2->port_length();
                            if (distance_cob != distance_net) {
                                continue;
                            }
                            candidates.emplace_back(CobPairCandidate{entry, exit, cob_to_cob_distance});
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
                throw std::runtime_error(std::format(
                    "select_entry_exit_candidates(): no candidates for net1='{}', net2='{}', overlap={}, checked_cob_pairs={}, keep_top_k={}",
                    net1->name(),
                    net2->name(),
                    format_region(r),
                    checked_pairs,
                    k
                ));
            }
            // show candidates
            for (const auto& c : candidates) {
                debug::info_fmt("select_entry_exit_candidates(): candidate=({}, {}, {}, {})", c.entry.row, c.entry.col, c.exit.row, c.exit.col);
            }

            return candidates;
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format(
                "select_entry_exit_candidates() failed: net1='{}', net2='{}', overlap={}, reason={}",
                net1 == nullptr ? std::String{"<null>"} : net1->name(),
                net2 == nullptr ? std::String{"<null>"} : net2->name(),
                format_region(overlap_region),
                err.what()
            ));
        }
    }

}

