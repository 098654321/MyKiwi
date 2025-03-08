#include "./sort.hh"
#include <global/debug/debug.hh>
#include <algorithm>


namespace kiwi::algo {

auto Sort::execute(hardware::Interposer* interposer, RouteEngine& engine, const RouteStrategy& strategy) const -> void {
    auto& nets = const_cast<std::Vector<circuit::Net*>&>(engine.nets());
    debug::debug("Sort by priority");
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
}

auto Sort::to_string() const -> const std::String {
    return "Sort";
}

}

