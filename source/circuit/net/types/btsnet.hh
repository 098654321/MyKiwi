#pragma once 

#include "../net.hh"
#include <std/collection.hh>
#include <hardware/cob/cob.hh>
#include <algo/router/common/maze/mazeroutestrategy.hh>


namespace kiwi::hardware {
    class Track;
    class Bump;
    class COB;
    class COBUnit;
}

namespace kiwi::circuit {

    class BumpToTracksNet : public Net {
    public:
        BumpToTracksNet(hardware::Bump* begin_bump, std::Vector<hardware::Track*> end_tracks, const std::HashSet<int>& modes);
        virtual ~BumpToTracksNet() noexcept override;

    public:
        virtual auto update_tob_postion(hardware::TOB* prev_tob, hardware::TOB* next_tob) -> void override;
        virtual auto route(hardware::Interposer* interposer, const algo::RouteStrategy& strategy) -> void override;
        virtual auto update_priority(float bias) -> void override;
        virtual auto coords() const -> std::Vector<hardware::Coord> override;
        virtual auto check_accessable_cobunit() -> void override;
        virtual auto accessable_cobunit() -> std::HashMap<hardware::Bump*, std::HashSet<std::usize>> override;
        virtual auto to_string() const -> std::String override;
        virtual auto port_number() const -> std::usize override;
        virtual auto search_related_nets(std::Vector<Net*>& nets) -> void override;
        virtual auto check_relativity(const hardware::Bump* node) const -> const Net* override;
        virtual auto check_relativity(const hardware::Track* node) const -> const Net* override;
        virtual auto connection_state() const -> std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>> override;
        virtual auto nodes_map() -> std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>> override;
        virtual auto nodes_direction() -> std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> override;
        virtual auto track_ports() const -> std::Pair<std::HashSet<hardware::Track*>, bool> override;

    public:
        virtual auto operator == (const Net& net) const -> bool override;

    public:
        auto begin_bump() const -> hardware::Bump* { return this->_begin_bump; }
        auto end_tracks() const -> const std::Vector<hardware::Track*>& { return this->_end_tracks; }

    private:
        hardware::Bump* _begin_bump;
        std::Vector<hardware::Track*> _end_tracks;
    };

}