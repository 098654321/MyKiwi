#pragma once

#include <hardware/interposer.hh>

#include <std/collection.hh>
#include <std/utility.hh>
#include <std/integer.hh>
#include <std/memory.hh>


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

class RouteEngine;
class HardwareRecorder;

struct CompareTrack {
    bool operator()(std::Pair<hardware::Track*, float> e1, std::Pair<hardware::Track*, float> e2) const {
        return e1.second < e2.second;
    }
};

struct IncreRouting {
    auto route_bump_to_bump_net(hardware::Interposer*, circuit::BumpToBumpNet*, RouteEngine&) const -> void;
    auto route_track_to_bump_net(hardware::Interposer*, circuit::TrackToBumpNet*, RouteEngine&) const -> void;
    auto route_bump_to_track_net(hardware::Interposer*, circuit::BumpToTrackNet*, RouteEngine&) const -> void;

    auto route_bump_to_bumps_net(hardware::Interposer*, circuit::BumpToBumpsNet*, RouteEngine&)  const -> void;
    auto route_track_to_bumps_net(hardware::Interposer*, circuit::TrackToBumpsNet*, RouteEngine&) const -> void;
    auto route_bump_to_tracks_net(hardware::Interposer*, circuit::BumpToTracksNet*, RouteEngine&) const -> void;

    auto route_tracks_to_bumps_net(hardware::Interposer*, circuit::TracksToBumpsNet*, RouteEngine&) const -> void;

    auto route_sync_net(hardware::Interposer*, circuit::SyncNet*, RouteEngine&) const -> void;

public:
    auto maze_search(
        hardware::Interposer* interposer, 
        const std::Vector<hardware::Track*>& begin_tracks,
        const std::HashSet<hardware::Track*>& end_tracks,
        const std::HashSet<hardware::Track*>& occupied_tracks,
        HardwareRecorder& recorder,
        bool reuse_type
    ) const -> algo::routed_path;

    auto route_path(
        hardware::Interposer* interposer, 
        const std::Vector<hardware::Track*>& begin_tracks,
        const std::HashSet<hardware::Track*>& end_tracks,
        const std::HashSet<hardware::Track*>& occupied_tracks,
        HardwareRecorder& recorder,
        bool reuse_type
    ) const -> algo::routed_path;

    template<class Node>
    // search current & existing resources
    auto searching_points( 
        Node*, circuit::Net*, hardware::Interposer*
    ) const -> std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>;

    auto set_tobconnector(
        std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>& map, hardware::Track* track,
        hardware::Bump* bump, circuit::PathPackage&
    ) const -> void;

    template <class Node>
    auto map_to_vec(
        Node* node, const std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>& map, HardwareRecorder& recorder, bool reuse_type
    ) const -> std::Vector<hardware::Track*>;

    auto map_to_set(const std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>& map) const -> std::HashSet<hardware::Track*>;
};

}

