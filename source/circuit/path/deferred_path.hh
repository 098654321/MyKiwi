#pragma once

#include "./pathpackage.hh"

namespace kiwi::circuit {

    // A deferred (POD) representation of a 3-segment constrained route:
    //   start -> entryCob, entryCob -> exitCob, exitCob -> end
    // It is intentionally hardware-state-free and can be converted to HistoryPathPackage,
    // then to PathPackage when we decide to commit.
    struct ThreeSegmentDeferredPath {
        // Optional endpoints (present for bump endpoints, absent for track endpoints).
        std::Option<std::Tuple<hardware::BumpCoord, TOBConnectorInfo, hardware::TrackCoord>> _tob_to_track;
        std::Option<std::Tuple<hardware::BumpCoord, TOBConnectorInfo, hardware::TrackCoord>> _track_to_tob;

        // Each segment uses the same schema as HistoryPathPackage::_regular_path:
        // (track_coord, connector_before_track)
        std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>> _seg1;
        std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>> _seg2;
        std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>> _seg3;

        // Total length (same meaning as PathPackage::_length). When computing from segments,
        // be careful to not double-count the duplicated junction track.
        std::size_t _length{0};

        auto to_history_pathpackage() const -> HistoryPathPackage;

        // Merge regular paths by enforcing that the last track of (a) equals the first of (b),
        // and dropping the duplicated first track from (b).
        static auto splice_regular(
            const std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>>& a,
            const std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>>& b
        ) -> std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>>;
    };

}

