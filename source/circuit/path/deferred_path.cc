#include "./deferred_path.hh"

#include <stdexcept>

namespace kiwi::circuit {

    auto ThreeSegmentDeferredPath::splice_regular(
        const std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>>& a,
        const std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>>& b
    ) -> std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>> {
        if (a.empty()) {
            return b;
        }
        if (b.empty()) {
            return a;
        }

        const auto& a_last = std::get<0>(a.back());
        const auto& b_first = std::get<0>(b.front());
        if (!(a_last == b_first)) {
            throw std::runtime_error("ThreeSegmentDeferredPath::splice_regular(): junction track mismatch");
        }

        auto out = std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>>{};
        out.reserve(a.size() + b.size() - 1);
        out.insert(out.end(), a.begin(), a.end());
        out.insert(out.end(), b.begin() + 1, b.end()); // drop duplicated junction track
        return out;
    }

    auto ThreeSegmentDeferredPath::to_history_pathpackage() const -> HistoryPathPackage {
        auto history = HistoryPathPackage{PathPackage{}};
        history._length = this->_length;

        auto merged12 = splice_regular(this->_seg1, this->_seg2);
        auto merged123 = splice_regular(merged12, this->_seg3);
        history._regular_path = std::move(merged123);

        history._tob_to_track.clear();
        history._track_to_tob.clear();
        if (this->_tob_to_track.has_value()) {
            history._tob_to_track.emplace_back(this->_tob_to_track.value());
        }
        if (this->_track_to_tob.has_value()) {
            history._track_to_tob.emplace_back(this->_track_to_tob.value());
        }

        return history;
    }

}

