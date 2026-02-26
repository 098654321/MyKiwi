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
        debug::info("Show bound bits:\n");

        this->_track_groups.show();
        debug::info("\n");

        this->_cob_groups.show();
        debug::info("\n");

        this->_tob_groups.show();
        debug::info("\n");
    }

    auto get_rate() const -> std::Tuple<double, double> {
        auto monopolized_by_reuse{0.0}, has_nonreuse{0.0};
        auto collect = [&](const auto& groups) {
            auto [group_monopolized_by_reuse, group_has_nonreuse] = groups.info();
            monopolized_by_reuse += group_monopolized_by_reuse;
            has_nonreuse += group_has_nonreuse;
        };

        collect(this->_track_groups);
        collect(this->_cob_groups);
        collect(this->_tob_groups);

        return std::make_tuple(monopolized_by_reuse, has_nonreuse);
    }

    auto show_rate() const -> void {
        auto [monopolized_by_reuse, has_nonreuse] = this->get_rate();
        auto sum = monopolized_by_reuse + has_nonreuse;
        
        debug::info_fmt(
            "Global registers: monopolized by reuse {}({}%), has nonreuse {}({}%)", 
             monopolized_by_reuse, 100*monopolized_by_reuse/sum, has_nonreuse, 100*has_nonreuse/sum
        );
    }

private:
    GlobalTrackGroups _track_groups;
    GlobalCOBGroups _cob_groups;
    GlobalTOBGroup _tob_groups;
};
    
}

