#include "./routeengine.hh"
#include <cassert>
#include <type_traits>



namespace kiwi::algo {

RouteEngine::RouteEngine(
    const std::HashMap<int, std::Vector<std::Rc<circuit::Net>>>& nets, const RouteStrategy& str, const AllocateStrategy& as, int m
)
    : _posi{0}, _routestrategy{str}, _allocator{as}, _mode{m}
{
    for (auto& [m, net_v]: nets) {
        auto res = this->_nets.emplace(m, std::Vector<circuit::Net*>{});

        for (auto& net: net_v) {
            res.first->second.emplace_back(net.get());
        }
    }
}

auto RouteEngine::routed_nets() const -> std::Vector<circuit::Net*> {
    assert(this->_posi < this->_nets.at(this->_mode).size());

    return std::Vector<circuit::Net*>(this->_nets.at(this->_mode).begin(), this->_nets.at(this->_mode).begin() + this->_posi);
}

}

