#include "./tbnet.hh"
#include <hardware/bump/bump.hh>


namespace kiwi::circuit {

    TrackToBumpNet::TrackToBumpNet(hardware::Track* begin_track, hardware::Bump* end_bump) :
        _begin_track{begin_track},
        _end_bump{end_bump},
        Net{Priority{3}}
    {
    }

    TrackToBumpNet::~TrackToBumpNet() noexcept {
    }

    auto TrackToBumpNet::update_tob_postion(hardware::TOB* prev_tob, hardware::TOB* next_tob) -> void {
        this->_end_bump = hardware::Bump::update_bump(this->_end_bump, prev_tob, next_tob);
    }

    auto TrackToBumpNet::route(hardware::Interposer* interposer, const algo::RouteStrategy& strategy) -> void {
        strategy.route_track_to_bump_net(interposer, this);
    }

    auto TrackToBumpNet::update_priority(float bias) -> void {
        assert(0 <= bias && bias < 1);
        this->_priority = Priority{this->_priority.value() + bias};
    }

    auto TrackToBumpNet::coords() const -> std::Vector<hardware::Coord> {
        return std::Vector<hardware::Coord>{this->begin_track()->coord(), this->end_bump()->coord()};
    }

    auto TrackToBumpNet::check_accessable_cobunit() -> void {
        std::usize index = _begin_track->coord().index;
        std::usize bank_size = hardware::COB::INDEX_SIZE/2;
        std::usize wilton_size = hardware::COBUnit::WILTON_SIZE;
        std::usize id {(index/bank_size)*wilton_size + (index%wilton_size)};
        
        std::HashSet<std::usize> cobunit_ids {id};
        _end_bump->intersect_access_unit(cobunit_ids);
    }

    auto TrackToBumpNet::accessable_cobunit() -> std::HashMap<hardware::Bump*, std::HashSet<std::usize>> {
        std::HashMap<hardware::Bump*, std::HashSet<std::usize>> map{};
        map.emplace(this->_end_bump, this->_end_bump->accessable_cobunit());
        return map;
    }

    
    auto TrackToBumpNet::to_string() const -> std::String {
        return std::format(
            "TrackToBumpNet: Begin track: '{}' to End bump '{}'", this->_begin_track->coord(), this->_end_bump->coord()
        );
    }

    auto TrackToBumpNet::port_number() const -> std::usize {
        return 2;
    }

    auto TrackToBumpNet::check_relativity(const hardware::Bump* node) const -> const Net* {
        if (this->_end_bump->coord() == node->coord()) {
            return this;
        }
        return nullptr;
    }

    auto TrackToBumpNet::check_relativity(const hardware::Track* node) const -> const Net* {
        if (this->_begin_track->coord() == node->coord()) {
            return this;
        }
        return nullptr;
    }

    auto TrackToBumpNet::search_related_nets(std::Vector<Net*>& nets) -> void {
        clear_related_nets();
        this->_related_nets_bump.emplace(this->_end_bump, search_nets_node<hardware::Bump>(this->_end_bump, nets));
        this->_related_nets_track.emplace(this->_begin_track, search_nets_node<hardware::Track>(this->_begin_track, nets));
    }

    auto TrackToBumpNet::connection_state() const -> std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>> {
        std::Vector<const hardware::Bump*> routable_bumps{}, unroutable_bumps{};
        std::Vector<const hardware::Track*> unroutable_tracks{};

        const auto& package = this->pathpackage();

        (package.find_bump(this->_end_bump).has_value() ? routable_bumps : unroutable_bumps).emplace_back(this->_end_bump);
        if (!package.find_track(this->_begin_track).has_value()) {
            unroutable_tracks.emplace_back(this->_begin_track);
        }

        return std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>> {
            routable_bumps, unroutable_bumps, unroutable_tracks
        };
    }

    auto TrackToBumpNet::nodes_map() -> std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>> {
        return std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>>{};
    }

    auto TrackToBumpNet::nodes_direction() -> std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> {
        return std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> {
            {this->_end_bump, hardware::TOBBumpDirection::TOBToBump}
        };
    }
}
