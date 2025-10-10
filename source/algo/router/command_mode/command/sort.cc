#include "./sort.hh"
#include <global/debug/debug.hh>
#include <algorithm>


namespace kiwi::algo {

auto Sort::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    auto nets = engine.nets();  
    auto compare = [] (circuit::Net* n1, circuit::Net* n2) -> bool {
        return n1->priority() > n2->priority();
    };

    if (engine.incremental()) {

    }
    else {
        // sort by port number in descending order
        debug::info("Sort by priority");

        float max_port_num {0};
        for (const auto& net : nets) {
            max_port_num = std::max(max_port_num, (float)net->port_number());
        }
        for (auto& net : nets) {
            net->update_priority(0.9 * ((float)net->port_number() / max_port_num));
        }
        std::sort(nets.begin(), nets.end(), compare);
        engine.update_net_seq(nets);

        // show sorted nets
        auto i {0};
        for (auto& n: nets) {
            debug::info_fmt("net {}: {}", i++, n->name());
        }
    }
}

auto Sort::to_string() const -> const std::String {
    return "Sort";
}

}

