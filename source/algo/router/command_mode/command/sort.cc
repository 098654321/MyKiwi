#include "./sort.hh"
#include <global/debug/debug.hh>
#include <algorithm>


namespace kiwi::algo {

auto Sort::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    auto nets = engine.nets();  //TODO：这里没有引用，没排上序

    if (engine.incremental()) {
        debug::info("Sort by reuse frequency");
    }
    else {
        // sort by port number in descending order
        debug::info("Sort by priority");
        std::usize max_port_num {0};
        for (const auto& net : nets) {
            max_port_num = std::max(max_port_num, net->port_number());
        }
        for (auto& net : nets) {
            net->update_priority(0.9*(net->port_number() / max_port_num));
        }
        auto compare = [] (circuit::Net* n1, circuit::Net* n2) -> bool {
            return n1->priority() > n2->priority();
        };
        std::sort(nets.begin(), nets.end(), compare);
        engine.update_net_seq(nets);
    }
    
    // show sorted nets
    auto i {0};
    for (auto& n: nets) {
        debug::info_fmt("net {}: {}", i++, n->name());
    }
}

auto Sort::to_string() const -> const std::String {
    return "Sort";
}

}

