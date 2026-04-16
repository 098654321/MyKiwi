#pragma once

#include <hardware/track/track.hh>
#include <hardware/bump/bump.hh>
#include <hardware/cob/cob.hh>
#include <hardware/cob/cobconnector.hh>
#include <hardware/tob/tobconnector.hh>
#include <hardware/tob/tob.hh>
#include <hardware/interposer.hh>
#include <global/std/collection.hh>
#include <global/std/utility.hh>
#include <global/debug/debug.hh>
#include <algorithm>
#include <optional>





namespace PR_tool::circuit {

// store the path temporarily with positive track sequence 
//! connector stored in package -> set state as "suspended". 
//! if connected, set state as "connected" and "given out". 
//! if removed from package, set state as "disconnected" and "stay inside"

struct COBConnectorInfo;
struct TOBConnectorInfo;
struct HistoryPathPackage;
struct PathInOrder;


struct PathPackage {
    PathPackage();
    PathPackage(const HistoryPathPackage& history_pathpackage, hardware::Interposer* pinterposer);

    auto show() const -> void;
    auto find_bump(const hardware::Bump* bump) const -> std::Option<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>>;
    auto find_track(const hardware::Track* track) const -> std::Option<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>>;
    auto reset_all() -> void;
    auto clear_all() -> void;
    auto occupy_all() -> void;
    auto connect_all() -> void;
    auto check_tobconenctor_consistency() const -> void;

    // track & the COBConnector before track 
    std::Vector<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>> _regular_path;
    std::Vector<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>>_tob_to_track;
    std::Vector<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>> _track_to_tob;
    std::usize _length;
};


// keep info in PathPackage & Plain of Data & can be transformed to PathPackage & constructed from PathPackage
struct HistoryPathPackage {
    HistoryPathPackage(const PathPackage& path_package);
    auto clear_all() -> void;

    std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>> _regular_path;
    std::Vector<std::Tuple<hardware::BumpCoord, TOBConnectorInfo, hardware::TrackCoord>>_tob_to_track;
    std::Vector<std::Tuple<hardware::BumpCoord, TOBConnectorInfo, hardware::TrackCoord>> _track_to_tob;
    std::size_t _length;
};


struct COBConnectorInfo {
    auto create_cobconnector(hardware::Interposer* pinterposer) const -> hardware::COBConnector;

    hardware::COBCoord cob_coord;
    hardware::COBDirection from_dir;
    std::usize from_track_index;
    hardware::COBDirection to_dir;
    std::usize to_track_index;
};


struct TOBConnectorInfo {
    auto create_tobconnector(hardware::Interposer* pinterposer) const -> hardware::TOBConnector;

    std::usize bump_index;
    std::usize hori_index;
    std::usize vert_index;
    std::usize track_index;
    hardware::TOBSignalDirection signal_dir;
    hardware::TOBCoord tob_coord;
};

struct PathInOrder {
    PathInOrder(
        std::optional<hardware::Bump*> head_bump, 
        std::optional<hardware::TOBConnector> head_connector, 
        std::optional<hardware::Bump*> tail_bump, 
        std::optional<hardware::TOBConnector> tail_connector, 
        const std::Vector<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>>& regular_path
    );

    std::optional<hardware::Bump*> _head_bump;
    std::optional<hardware::TOBConnector> _head_connector;
    std::optional<hardware::Bump*> _tail_bump;
    std::optional<hardware::TOBConnector> _tail_connector;
    std::Vector<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>> _regular_path;
};


}
