#include "./bbnet.hh"
#include "std/string.hh"
#include <std/format.hh>
#include <hardware/bump/bump.hh>


namespace kiwi::circuit {

    BumpToBumpNet::BumpToBumpNet(hardware::Bump* begin_bump, hardware::Bump* end_bump) :
        _begin_bump{begin_bump},
        _end_bump{end_bump},
        Net{Priority{4}}
    {
    }

    BumpToBumpNet::~BumpToBumpNet() noexcept {
    }

    auto BumpToBumpNet::update_tob_postion(hardware::TOB* prev_tob, hardware::TOB* next_tob) -> void {
        this->_begin_bump = hardware::Bump::update_bump(this->_begin_bump, prev_tob, next_tob);
        this->_end_bump = hardware::Bump::update_bump(this->_end_bump, prev_tob, next_tob);
    }

    auto BumpToBumpNet::route(hardware::Interposer* interposer, const algo::RouteStrategy& strategy) -> void {
        strategy.route_bump_to_bump_net(interposer, this);
    }

    auto BumpToBumpNet::update_priority(float bias) -> void {
        assert(0 <= bias && bias < 1);
        this->_priority = Priority{this->_priority.value() + bias};
    }

    auto BumpToBumpNet::coords() const -> std::Vector<hardware::Coord> {
        return std::Vector<hardware::Coord>{this->begin_bump()->coord(), this->end_bump()->coord()};
    }

    auto BumpToBumpNet::check_accessable_cobunit() -> void {
        std::HashSet<std::usize> accessable_cobunit {};
        for (std::usize i = 0; i < hardware::COB::UNIT_SIZE; ++i){
            accessable_cobunit.emplace(i);
        }
        _begin_bump->intersect_access_unit(accessable_cobunit);
        _end_bump->intersect_access_unit(accessable_cobunit);
    }

    auto BumpToBumpNet::accessable_cobunit() -> std::HashMap<hardware::Bump*, std::HashSet<std::usize>> {
        std::HashMap<hardware::Bump*, std::HashSet<std::usize>> map {};

        map.emplace(this->_begin_bump, this->_begin_bump->accessable_cobunit());
        map.emplace(this->_end_bump, this->_end_bump->accessable_cobunit());
        return map;
    }

    auto BumpToBumpNet::port_number() const -> std::usize {
        return 2;
    }

    auto BumpToBumpNet::to_string() const -> std::String {
        return std::format(
            "BumpToBumpNet: Begin bump: '{}' to End bump '{}'", this->_begin_bump->coord(), this->_end_bump->coord()
        );
    }

    auto BumpToBumpNet::check_relativity(const hardware::Bump* node) const -> const Net* {
        if (node->coord() == this->_begin_bump->coord() || node->coord() == this->_end_bump->coord()) {
            return dynamic_cast<const Net*>(this);
        }
        else {
            return nullptr;
        }
    }

    auto BumpToBumpNet::search_related_nets(std::Vector<Net*>& nets) -> void {
        clear_related_nets();
        this->_related_nets_bump.emplace(this->_begin_bump, search_nets_node<hardware::Bump>(this->_begin_bump, nets));
        this->_related_nets_bump.emplace(this->_end_bump, search_nets_node<hardware::Bump>(this->_end_bump, nets));
    }

    auto BumpToBumpNet::connection_state() const -> std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>> {
        std::Vector<const hardware::Bump*> routable_bumps {}, unroutable_bumps {};

        const auto& package = this->pathpackage();

        auto classify_bump = [&](const hardware::Bump* bump) {
            (package.find_bump(bump).has_value() ? routable_bumps : unroutable_bumps).emplace_back(bump);
        };
        classify_bump(this->_begin_bump);
        classify_bump(this->_end_bump);

        return std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>>{
            routable_bumps, unroutable_bumps, std::Vector<const hardware::Track*>{}
        };
    }

    auto BumpToBumpNet::nodes_map() -> std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>> {
        return std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>> {
            {this->_begin_bump, std::HashSet<hardware::Bump*>{this->_end_bump}}
        };
    }

    auto BumpToBumpNet::nodes_direction() -> std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> {
        return std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> {
            {this->_begin_bump, hardware::TOBBumpDirection::BumpToTOB},
            {this->_end_bump, hardware::TOBBumpDirection::TOBToBump}
        };
    }

}
