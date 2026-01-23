#pragma once

#include "../routestrategy.hh"
#include "./mazererouter.hh"

#include <hardware/cob/cob.hh>
#include <hardware/tob/tob.hh>
#include <hardware/bump/bump.hh>
#include <hardware/track/track.hh>

#include <std/collection.hh>
#include <std/utility.hh>
#include <std/integer.hh>
#include <std/memory.hh>



namespace kiwi::circuit {
    struct PathPackage;
    class Net;
}

namespace kiwi::algo {

    using routed_path = std::Vector<std::Tuple<kiwi::hardware::Track*, std::Option<kiwi::hardware::COBConnector>>>;

    class MazeRouteStrategy : public RouteStrategy {
    public:
        MazeRouteStrategy(bool incremental = false): _rerouter(std::make_unique<MazeRerouter>(MazeRerouter{incremental})) {}

    public:
        virtual auto route_bump_to_bump_net(hardware::Interposer*, circuit::BumpToBumpNet*)  const -> void override;
        virtual auto route_track_to_bump_net(hardware::Interposer*, circuit::TrackToBumpNet*) const -> void override;
        virtual auto route_bump_to_track_net(hardware::Interposer*, circuit::BumpToTrackNet*) const -> void override;

        virtual auto route_bump_to_bumps_net(hardware::Interposer*, circuit::BumpToBumpsNet*)  const -> void override;
        virtual auto route_track_to_bumps_net(hardware::Interposer*, circuit::TrackToBumpsNet*) const -> void override;
        virtual auto route_bump_to_tracks_net(hardware::Interposer*, circuit::BumpToTracksNet*) const -> void override;

        virtual auto route_tracks_to_bumps_net(hardware::Interposer*, circuit::TracksToBumpsNet*) const -> void override;

        virtual auto route_sync_net(hardware::Interposer*, circuit::SyncNet*) const -> void override;
    
    private:
        template<class InputNode, class OutputNode>
        auto route_node_to_node_net(
            hardware::Interposer* interposer, 
            std::Pair<InputNode*, std::Vector<hardware::Track*>>, std::Pair<OutputNode*, std::HashSet<hardware::Track*>> 
        ) const -> circuit::PathPackage;
    
    // simple routing functions
    private:
        auto maze_search(
            hardware::Interposer* interposer, 
            const std::Vector<hardware::Track*>& begin_tracks,
            const std::HashSet<hardware::Track*>& end_tracks,
            const std::HashSet<hardware::Track*>& occupied_tracks
        ) const -> algo::routed_path;
    
        auto route_path(
            hardware::Interposer* interposer, 
            const std::Vector<hardware::Track*>& begin_tracks,
            const std::HashSet<hardware::Track*>& end_tracks,
            const std::HashSet<hardware::Track*>& occupied_tracks
        ) const -> algo::routed_path;

        auto track_map_to_track_vec(
            const std::HashMap<hardware::Track*, 
            hardware::TOBConnector>& map,
            const std::Array<std::usize, hardware::COB::UNIT_SIZE>& cobunit_usage
        ) const -> std::Vector<hardware::Track*>;

        auto track_map_to_track_set(
            const std::HashMap<hardware::Track*, 
            hardware::TOBConnector>& map
        ) const -> std::HashSet<hardware::Track*>;

    // synchrnized rouitng functions
    private:
        // first round of routing & collecting paths 
        template <class Net>
        auto sync_preroute(
            hardware::Interposer* interposer,
            std::Vector<std::Rc<Net>>& sync_net,
            std::HashSet<hardware::Track*>& occupied_tracks_vec
        ) const -> std::usize;

        // the path is already stored in ascending order with vector index when return 
        auto sync_reroute(
            hardware::Interposer* interposer,
            std::Vector<circuit::Net*>& nets,
            std::usize max_length
        ) const -> std::tuple<bool, std::usize>;
    
    private:
        std::Box<MazeRerouter> _rerouter;
    };

    template <class Node>
    auto existing_path_vec(Node*, circuit::Net*) -> std::Vector<hardware::Track*>;

    template <class Node>
    auto existing_path_set(Node*, circuit::Net*) -> std::HashSet<hardware::Track*>;


}
