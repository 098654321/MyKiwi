#pragma once

#include <hardware/interposer.hh>
#include <algo/router/common/maze/mazeroutestrategy.hh>
#include <std/collection.hh>
#include <std/utility.hh>
#include <std/integer.hh>
#include <std/memory.hh>
#include <algo/router/common/maze/track_comparator.hh>
#include <algo/router/multi_mode/occupancy_view.hh>


namespace kiwi::circuit {
    class Net;
    class BumpToBumpNet;
    class TrackToBumpNet;
    class BumpToTrackNet;
    class BumpToBumpsNet;
    class TrackToBumpsNet;
    class BumpToTracksNet;
    class TracksToBumpsNet;
    class SyncNet;

    class PathPackage;
}


namespace kiwi::algo {

using routed_path = std::Vector<std::Tuple<kiwi::hardware::Track*, std::Option<kiwi::hardware::COBConnector>>>;

    auto route_bump_to_bump_net_multi_mode(hardware::Interposer*, circuit::BumpToBumpNet*, algo::OccupancyView& view, HardwareRecorder& recorder, int mode) -> bool;
    auto route_track_to_bump_net_multi_mode(hardware::Interposer*, circuit::TrackToBumpNet*, algo::OccupancyView& view, HardwareRecorder& recorder, int mode) -> bool;
    auto route_bump_to_track_net_multi_mode(hardware::Interposer*, circuit::BumpToTrackNet*, algo::OccupancyView& view, HardwareRecorder& recorder, int mode) -> bool;

    auto route_bump_to_bumps_net_multi_mode(hardware::Interposer*, circuit::BumpToBumpsNet*, algo::OccupancyView& view, HardwareRecorder& recorder, int mode) -> bool;
    auto route_track_to_bumps_net_multi_mode(hardware::Interposer*, circuit::TrackToBumpsNet*, algo::OccupancyView& view, HardwareRecorder& recorder, int mode) -> bool;
    auto route_bump_to_tracks_net_multi_mode(hardware::Interposer*, circuit::BumpToTracksNet*, algo::OccupancyView& view, HardwareRecorder& recorder, int mode) -> bool;

    auto route_tracks_to_bumps_net_multi_mode(hardware::Interposer*, circuit::TracksToBumpsNet*, algo::OccupancyView& view, HardwareRecorder& recorder, int mode) -> bool;

    auto route_sync_net_multi_mode(hardware::Interposer*, circuit::SyncNet*, algo::OccupancyView& view, HardwareRecorder& recorder, int mode) -> bool;

    auto maze_search_multi_mode(
        hardware::Interposer* interposer, 
        const std::Vector<hardware::Track*>& begin_tracks,
        const std::HashSet<hardware::Track*>& end_tracks,
        const std::HashSet<hardware::Track*>& occupied_tracks,
        const algo::OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode
    ) -> algo::routed_path;

    auto route_path_multi_mode(
        hardware::Interposer* interposer, 
        const std::Vector<hardware::Track*>& begin_tracks,
        const std::HashSet<hardware::Track*>& end_tracks,
        const std::HashSet<hardware::Track*>& occupied_tracks,
        algo::OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode
    ) -> algo::routed_path;

    auto sync_preroute_bump_to_bump_multi_mode(
        hardware::Interposer* interposer,
        std::Vector<std::Rc<circuit::BumpToBumpNet>>& sync_net,
        std::HashSet<hardware::Track*>& occupied_tracks_vec,
        algo::OccupancyView& view,
        HardwareRecorder& recorder,
        int mode
    ) -> std::usize;

    auto sync_preroute_bump_to_track_multi_mode(
        hardware::Interposer* interposer,
        std::Vector<std::Rc<circuit::BumpToTrackNet>>& sync_net,
        std::HashSet<hardware::Track*>& occupied_tracks_vec,
        algo::OccupancyView& view,
        HardwareRecorder& recorder,
        int mode
    ) -> std::usize;

    auto sync_preroute_track_to_bump_multi_mode(
        hardware::Interposer* interposer,
        std::Vector<std::Rc<circuit::TrackToBumpNet>>& sync_net,
        std::HashSet<hardware::Track*>& occupied_tracks_vec,
        algo::OccupancyView& view,
        HardwareRecorder& recorder,
        int mode
    ) -> std::usize;

    auto sync_incremental_reroute_multi_mode(
        hardware::Interposer* interposer,
        std::Vector<circuit::Net*>& nets,
        std::usize max_length,
        algo::OccupancyView& view,
        HardwareRecorder& recorder,
        int mode
    ) -> std::tuple<bool, std::usize>;

    template<class Node>
    // search current & existing resources
    auto searching_points_multi_mode( 
        Node*, circuit::Net*, hardware::Interposer*, const algo::OccupancyView& view, int mode
    ) -> std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>;

    auto set_tobconnector_multi_mode(
        std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>& map, hardware::Track* track,
        hardware::Bump* bump, circuit::PathPackage&, bool head
    ) -> void;

    template <class Node>
    auto map_to_vec_multi_mode(
        Node* node, const std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>& map, HardwareRecorder& recorder, bool reuse_type
    ) -> std::Vector<hardware::Track*>;

    template <class Node>
    auto map_to_vec_multi_mode(
        Node* node, const std::HashMap<hardware::Track*, hardware::TOBConnector>& map, HardwareRecorder& recorder, bool reuse_type
    ) -> std::Vector<hardware::Track*>;

    auto map_to_set_multi_mode(const std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>& map) -> std::HashSet<hardware::Track*>;

    auto map_to_set_multi_mode(const std::HashMap<hardware::Track*, hardware::TOBConnector>& map) -> std::HashSet<hardware::Track*>;

}



