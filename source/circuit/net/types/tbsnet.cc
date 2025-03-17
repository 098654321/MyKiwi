#include "./tbsnet.hh"
#include <hardware/bump/bump.hh>
#include <algorithm>


namespace kiwi::circuit {

    TrackToBumpsNet::TrackToBumpsNet(hardware::Track* begin_track, std::Vector<hardware::Bump*> end_bumps) :
        _begin_track{begin_track},
        _end_bumps{std::move(end_bumps)},
        Net{Priority{1}}
    {
    }

    TrackToBumpsNet::~TrackToBumpsNet() noexcept {

    }

    auto TrackToBumpsNet::update_tob_postion(hardware::TOB* prev_tob, hardware::TOB* next_tob) -> void {
        for (auto& bump : this->_end_bumps) {
            bump = hardware::Bump::update_bump(bump, prev_tob, next_tob);
        }
    }

    auto TrackToBumpsNet::route(hardware::Interposer* interposer, const algo::RouteStrategy& strategy) -> void {
        strategy.route_track_to_bumps_net(interposer, this);
    }

    auto TrackToBumpsNet::update_priority(float bias) -> void {
        assert(0 <= bias && bias < 1);
        this->_priority = Priority{this->_priority.value() + bias};
    }

    auto TrackToBumpsNet::coords() const -> std::Vector<hardware::Coord> {
        auto coords = std::Vector<hardware::Coord>{};
        for (auto bump : this->end_bumps()) {
            coords.emplace_back(bump->coord());
        }
        coords.emplace_back(this->begin_track()->coord());
        return coords;
    }

    auto TrackToBumpsNet::check_accessable_cobunit() -> void {
        std::usize index = _begin_track->coord().index;
        std::usize bank_size = hardware::COB::INDEX_SIZE/2;
        std::usize wilton_size = hardware::COBUnit::WILTON_SIZE;
        std::usize id {(index/bank_size)*wilton_size + (index%wilton_size)};
        
        std::HashSet<std::usize> cobunit_ids {id};
        for (auto bump: _end_bumps){
            bump->intersect_access_unit(cobunit_ids);
        }
    }

    auto TrackToBumpsNet::accessable_cobunit() -> std::HashMap<hardware::Bump*, std::HashSet<std::usize>> {
        std::HashMap<hardware::Bump*, std::HashSet<std::usize>> map{};
        for(auto& b: this->_end_bumps) {
            map.emplace(b, b->accessable_cobunit());
        }
        return map;
    }

    auto TrackToBumpsNet::to_string() const -> std::String {
        auto ss = std::StringStream {};
        ss << std::format("TrackToBumpsNet: Begin track '{}' to End bumps '[", this->_begin_track->coord());
        for (int i = 0; i < this->_end_bumps.size(); ++i) {
            if (i != 0) {
                ss << ", ";
            }
            ss << std::format("{}", this->_end_bumps[i]->coord());
        }
        ss << ']';
        return ss.str();
    }

    auto TrackToBumpsNet::port_number() const -> std::usize {
        return (this->_end_bumps.size() + 1);
    }

    auto TrackToBumpsNet::check_relativity(const hardware::Track* node) const -> const Net* {
        if (this->_begin_track->coord() == node->coord()) {
            return this;
        }
        return nullptr;
    }

    auto TrackToBumpsNet::check_relativity(const hardware::Bump* node) const -> const Net* {
        for (auto bump : this->_end_bumps) {
            if (bump->coord() == node->coord()) {
                return this;
            }
        }
        return nullptr;
    }

    auto TrackToBumpsNet::search_related_nets(std::Vector<Net*>& nets) -> void {
        clear_related_nets();
        this->_related_nets_track.emplace(this->_begin_track, search_nets_node<hardware::Track>(this->_begin_track, nets));
        for (auto bump : this->_end_bumps) {
            this->_related_nets_bump.emplace(bump, search_nets_node<hardware::Bump>(bump, nets));
        }
    }

    auto TrackToBumpsNet::connection_state() const -> std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>> {
        std::vector<const hardware::Bump*> routable_bumps{}, unroutable_bumps{};
        std::Vector<const hardware::Track*> unroutable_tracks{};

        const auto& package = this->pathpackage();
        if (!package.find_track(this->_begin_track).has_value()) {
            unroutable_tracks.emplace_back(this->_begin_track);
        }

        auto classify_bump = [&](hardware::Bump* bump) {
            (package.find_bump(bump).has_value() ? routable_bumps : unroutable_bumps).emplace_back(bump);
        };
        std::for_each(this->_end_bumps.begin(), this->_end_bumps.end(), classify_bump);

        return std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>> {
            routable_bumps, unroutable_bumps, unroutable_tracks
        };
    }

    auto TrackToBumpsNet::nodes_map() -> std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>> {
        return std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>> {};
    }

    auto TrackToBumpsNet::nodes_direction() -> std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> {
        std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> map{};

        for (auto& b: this->_end_bumps) {
            map.emplace(b, hardware::TOBBumpDirection::TOBToBump);
        }
        return map;
    }

}
