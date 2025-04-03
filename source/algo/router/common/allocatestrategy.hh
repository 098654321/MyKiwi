#pragma once

#include <global/debug/debug.hh>
#include <hardware/interposer.hh>
#include <circuit/net/nets.hh>


namespace kiwi::hardware {
    class Interposer;
}

namespace kiwi::circuit {
    class Net;
}

namespace kiwi::algo {

struct AllocateStrategy{

    /*
    The process of mapping bump to track can be modeled as a "Maximum Bipartite Matching Problem",
    given that there is unique solution of the combination of tob-mux when bump & track index is determined.
    */ 

    virtual auto allocate(hardware::Interposer* interposer, std::Vector<circuit::Net*>& nets) const -> void = 0;
    
};

}
