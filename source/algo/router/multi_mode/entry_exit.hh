#pragma once

#include <circuit/net/net.hh>
#include <circuit/path/region.hh>
#include <hardware/cob/cobcoord.hh>
#include <std/collection.hh>
#include <std/integer.hh>

namespace kiwi::algo::multi_mode {

    struct CobPairCandidate {
        hardware::COBCoord entry;
        hardware::COBCoord exit;
        std::i64 cob_to_cob_dist{0};
    };

    auto select_entry_exit_candidates(
        const circuit::Net& net1,
        const circuit::Net& net2,
        const circuit::Region& overlap_region,
        std::usize k
    ) -> std::Vector<CobPairCandidate>;

}

