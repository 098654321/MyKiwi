#include "./routeengine.hh"
#include <cassert>
#include <type_traits>
#include <ranges>
#include <algorithm>


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
    this->_incre_strategy.set_recorder(&this->_recorder);
}

auto RouteEngine::routed_nets() const -> std::Vector<circuit::Net*> {
    assert(this->_posi < this->all_nets().size());

    std::Vector<circuit::Net*> res {};
    std::usize index = 0;
    for (auto& [mode, nets]: this->_nets) {
        if (index >= this->_posi) {
            break;
        }
        for (auto& net: nets) {
            if (index >= this->_posi) {
                break;
            }
            res.emplace_back(net);
            index += 1;
        }
    }
    return res;
}

auto RouteEngine::update_net_seq(std::Vector<circuit::Net*>& nets) -> void {
    this->_nets.at(this->_mode) = nets;
}

auto RouteEngine::nets() const -> std::Vector<circuit::Net*> {
    if (this->_incremental) {
        auto all_nets = std::Vector<circuit::Net*> {};
        auto s = std::Set<circuit::Net*> {};
        for (auto& [m, net_v]: this->_nets) {
            s.insert(net_v.begin(), net_v.end());
        }
        for (auto& [m, net_v]: this->_nets) {
            for (auto& net: net_v) {
                if (s.contains(net)) {
                    all_nets.emplace_back(net);
                    s.erase(net);
                }
            }
        }

        if (!this->_path_exists) {
            // no existing path, return all nets
            return all_nets;
        }
        else {
            // return nets without existing path
            auto iter = std::remove_if(all_nets.begin(), all_nets.end(), [](auto& net) {
                return net->pathpackage()._regular_path.size() != 0;
            });
            all_nets.erase(iter, all_nets.end());
            return all_nets;
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

