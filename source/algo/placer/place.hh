#pragma once

#include <std/memory.hh>
#include <std/collection.hh>
#include <std/integer.hh>
#include <circuit/topdieinst/topdieinst.hh>

namespace PR_tool::hardware {
    class Interposer;
}

namespace PR_tool::circuit {
    class BaseDie;
}

namespace PR_tool::algo {
    struct PlaceStrategy;
    auto place(
        hardware::Interposer* interposer,
        std::Vector<circuit::TopDieInstance*>& topdies,
        const PlaceStrategy& strategy
    ) -> void;
    
    // BaseDie
    auto place(
        hardware::Interposer* interposer,
        std::Vector<circuit::TopDieInstance*>& topdies,
        circuit::BaseDie* basedie,
        const PlaceStrategy& strategy
    ) -> void;   

    auto check_nets(
        const std::Vector<circuit::TopDieInstance*>& topdies
    ) -> void;

    auto evaluate_placement(
        hardware::Interposer* interposer,
        const std::Vector<circuit::TopDieInstance*>& topdies,
        circuit::BaseDie* basedie,
        const PlaceStrategy& strategy
    ) -> std::i64;
}