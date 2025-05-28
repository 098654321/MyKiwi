#pragma once

#include "./track_bits.hh"
#include "./cob_bits.hh"
#include "./tob_bits.hh"


namespace kiwi::algo {

class GlobalBoundBits {
public:
    GlobalBoundBits() : _track_groups{}, _cob_groups{}, _tob_groups{} {}
    ~GlobalBoundBits() = default;

public:
    auto record_track(const hardware::TrackCoord& coord, bool reuse_type) -> void {
        this->_track_groups.record_track(coord, reuse_type);
    }
    
    auto record_cob(const hardware::COBConnector& connector, bool reuse_type) -> void {
        this->_cob_groups.record_cob(connector, reuse_type);
    }

    auto record_tob(const hardware::TOBCoord& coord, const hardware::TOBConnector& connector, bool reuse_type) -> void {
        this->_tob_groups.record_tob(coord, connector, reuse_type);
    }

    auto show_bits() -> void {
        debug::debug("Show bound bits:\n");

        this->_track_groups.show();
        debug::debug("\n");

        this->_cob_groups.show();
        debug::debug("\n");

        this->_tob_groups.show();
        debug::debug("\n");
    }

    auto show_rate() -> void {
        auto not_used{0.0}, monopolized{0.0}, mixed{0.0};
        auto collect = [&](auto& groups) {
            auto [group_not_used, group_monopolized, group_mixed] = groups.info();
            not_used += group_not_used;
            monopolized += group_monopolized;
            mixed += group_mixed;
        };

        collect(this->_track_groups);
        collect(this->_cob_groups);
        collect(this->_tob_groups);

        auto sum = monopolized + mixed;
        debug::info_fmt(
            "Global registers: monopolized {}%, mixed {}%", 100*monopolized/sum, 100*mixed/sum
        );
    }

private:
    GlobalTrackGroups _track_groups;
    GlobalCOBGroups _cob_groups;
    GlobalTOBGroup _tob_groups;
};
    
}

