#include "./place.hh"
#include "./placestrategy.hh"
#include "debug/debug.hh"
#include <circuit/basedie.hh>
#include <circuit/topdieinst/topdieinst.hh>
#include <circuit/net/nets.hh>
#include <hardware/tob/tob.hh>
#include <hardware/bump/bump.hh>
#include <std/collection.hh>

namespace kiwi::algo {
    auto place(
        hardware::Interposer* interposer,
        std::Vector<circuit::TopDieInstance*>& topdies,
        const PlaceStrategy& strategy
    )-> void {
        place(interposer, topdies, nullptr, strategy);
    }

    auto place(
        hardware::Interposer* interposer,
        std::Vector<circuit::TopDieInstance*>& topdies,
        circuit::BaseDie* basedie,
        const PlaceStrategy& strategy
    )-> void {
        if (!interposer) {
            debug::error("Interposer pointer is empty");
            return;
        }
        if (topdies.empty()) {
            debug::warning("No top-level chip instance needs to be laid out.");
            return;
        }
        if (!basedie) {
            debug::warning("BaseDie pointer is empty, limited functionality");
        }

        debug::info_fmt("Placement driver: {} top-level instances", topdies.size());
        for (auto* td : topdies) {
            auto tob = td ? td->tob() : nullptr;
            if (tob) {
                debug::info_fmt("Placement driver: {} at TOB {}", td->name(), tob->coord());
            } else {
                debug::info_fmt("Placement driver: {} at TOB {}", td->name(), "None");
            }
        }
        
        strategy.place(interposer, topdies);
        check_nets(topdies);

        auto cost = evaluate_placement(interposer, topdies, basedie, strategy);
    }
    
    auto evaluate_placement(
        hardware::Interposer* interposer,
        const std::Vector<circuit::TopDieInstance*>& topdies,
        circuit::BaseDie* basedie,
        const PlaceStrategy& strategy
    ) -> std::i64 {
        return strategy.evaluate_placement(interposer, topdies, basedie);
    }

    auto check_nets(
        const std::Vector<circuit::TopDieInstance*>& topdies
    ) -> void {
        auto nets = std::HashSet<circuit::Net*>{};
        for (auto* td : topdies) {
            for (auto* net : td->nets()) {
                nets.emplace(net);
            }
        }
        std::usize suspicious_count {0};
        for (auto* net : nets) {
            if (auto* syncn = dynamic_cast<circuit::SyncNet*>(net)) {
                for (auto& r : syncn->btbnets()) {
                    auto* b = r.get();
                    auto* bb = b->begin_bump();
                    auto* eb = b->end_bump();
                    if (bb && eb && bb->tob()->coord() == eb->tob()->coord()) {
                        ++suspicious_count;
                        debug::warning_fmt(
                            "Placement driver: Sync BumpToBumpNet {} on same TOB {}, begin {}, end {}",
                            b->name(), bb->tob()->coord(), bb->coord(), eb->coord()
                        );
                    }
                }
            } else if (auto* btb = dynamic_cast<circuit::BumpToBumpNet*>(net)) {
                auto* bb = btb->begin_bump();
                auto* eb = btb->end_bump();
                if (bb && eb && bb->tob()->coord() == eb->tob()->coord()) {
                    ++suspicious_count;
                    debug::warning_fmt(
                        "Placement driver: BumpToBumpNet {} on same TOB {}, begin {}, end {}",
                        btb->name(), bb->tob()->coord(), bb->coord(), eb->coord()
                    );
                }
            }
        }
        if (suspicious_count > 0) {
            debug::warning_fmt("Placement driver: {} suspicious BumpToBump nets found on same TOB", suspicious_count);
        }
    }
}
