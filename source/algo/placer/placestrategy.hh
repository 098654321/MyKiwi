#pragma once

#include <std/memory.hh>
#include <std/collection.hh>
#include <circuit/topdieinst/topdieinst.hh>

namespace PR_tool::hardware {
    class Interposer;
}

namespace PR_tool::circuit {
    class TopDieInstance;
    class BaseDie;
}

namespace PR_tool::algo {
    struct PlaceStrategy {
        virtual auto place(
            hardware::Interposer* interposer,
            std::Vector<circuit::TopDieInstance*>& topdies
        ) const -> void = 0;
        
        virtual auto evaluate_placement(
            hardware::Interposer* interposer,
            const std::Vector<circuit::TopDieInstance*>& topdies,
            circuit::BaseDie* basedie
        ) const -> std::i64 = 0;
    };
}