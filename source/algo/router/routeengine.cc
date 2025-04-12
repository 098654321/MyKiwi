#include "./routeengine.hh"
#include <cassert>
#include <type_traits>
#include <ranges>


namespace kiwi::algo {

RouteEngine::RouteEngine(
    const std::HashMap<int, std::Vector<std::Rc<circuit::Net>>>& nets, const RouteStrategy& str, const AllocateStrategy& as, int m,
    bool incremental, bool path_exists, hardware::Interposer* interposer
)
    : _posi{0}, _routestrategy{str}, _allocator{as}, _mode{m}, _incremental{incremental}, _path_exists{path_exists}, _incre_strategy{}, _recorder{interposer}
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

auto RouteEngine::nets() const -> std::Vector<circuit::Net*> {
    if (this->_incremental) {
        auto all_nets = std::Set<circuit::Net*> {};
        for (auto& [m, net_v]: this->_nets) {
            all_nets.insert(net_v.begin(), net_v.end());
        }

        if (!this->_path_exists) {
            // no existing path, return all nets
            return std::Vector<circuit::Net*>(all_nets.begin(), all_nets.end());
        }
        else {
            // return nets without existing path
            auto res = std::Vector<circuit::Net*>{};
            for (auto& net: all_nets) {
                if (net->pathpackage()._regular_path.size() == 0) {
                    res.emplace_back(net);
                }
            }
            return res;
        }
    }
    else {
        return this->_nets.at(this->_mode);
    }
}

auto RouteEngine::all_nets() const -> std::Vector<circuit::Net*> {
    std::Set<circuit::Net*> res {};
    for (auto& [m, net_v]: this->_nets) {
        res.insert(net_v.begin(), net_v.end());
    }
    return std::Vector<circuit::Net*>(res.begin(), res.end());
}

auto RouteEngine::reusable_nets() const -> std::Set<circuit::Net*> {
    std::Set<circuit::Net*> res {};
    for (auto& [m, net_v]: this->_nets) {
        for (auto& net: net_v) {
            if (net->modes().size() > 1) {
                res.insert(net);
            }
        }
    }
    return res;
}

auto RouteEngine::non_reusable_nets() const -> std::Set<circuit::Net*> {
    std::Set<circuit::Net*> res {};
    for (auto& [m, net_v]: this->_nets) {
        for (auto& net: net_v) {
            if (net->modes().size() == 1) {
                res.insert(net);
            }
        }
    }
    return res;
}

}

