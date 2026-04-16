#pragma once

#include <global/debug/debug.hh>
#include <hardware/interposer.hh>
#include <circuit/net/nets.hh>


namespace PR_tool::hardware {
    class Interposer;
}

namespace PR_tool::circuit {
    class Net;
}

namespace PR_tool::algo {

struct AllocateStrategy{

    /*
    The process of mapping bump to track can be modeled as a "Maximum Bipartite Matching Problem",
    given that there is unique solution of the combination of tob-mux when bump & track index is determined.
    */ 

    virtual auto allocate(hardware::Interposer* interposer, std::Vector<circuit::Net*> nets) const -> void = 0;
    
};

}
