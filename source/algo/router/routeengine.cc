#include "./routeengine.hh"
#include <cassert>
#include <type_traits>



namespace kiwi::algo {

RouteEngine::RouteEngine(
    const std::Vector<std::Box<circuit::Net>>& nets, const RouteStrategy& str, const AllocateStrategy& as
)
    : _posi{0}, _routestrategy{str}, _allocator{as}
{
    for (auto& net: nets) {
        this->_nets.emplace_back(net.get());
    }
}

auto RouteEngine::routed_nets() const -> std::Vector<circuit::Net*> {
    assert(this->_posi < this->_nets.size());

    return std::Vector<circuit::Net*>(this->_nets.begin(), this->_nets.begin() + this->_posi);
}

}

