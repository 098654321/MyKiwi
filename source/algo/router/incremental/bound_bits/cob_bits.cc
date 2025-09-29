#include "./cob_bits.hh"
#include <ranges>


namespace kiwi::algo {

// record "to_track" in cobconnector
auto COBGroup::record_cobgroup(const hardware::COBConnector& connector, bool reuse_type) -> void {
    auto& group = this->_groups.emplace(connector.to_dir(), std::Array<BitsGroup<COBGroupSize>, COBGroupNum>{}).first->second;
    auto [group_index, group_input] = this->trackindex_to_groupinfo(connector.from_track_index());
    reuse_type ? group.at(group_index).add_reuse_number() : group.at(group_index).add_nonreuse_number();
}

auto COBGroup::trackindex_to_groupinfo(std::usize track_index) const -> std::Tuple<std::usize, std::usize> {
    return std::Tuple<std::usize, std::usize> {
        track_index / COBGroupSize,
        track_index % COBGroupSize,
    };
}

auto COBGroup::to_string() const -> std::String {
    std::String message {};
    for (auto& [direction, groups]: this->_groups) {
        message += this->cobdirection_to_string(direction);
        message += ": ";
        for (auto i: std::views::iota(0, (int)COBGroupNum)) {
            auto group = groups.at(i);
            message += std::format("group {}, {}\n", i, group.to_string());
        }
    }
    return message;
}

auto COBGroup::cobdirection_to_string(hardware::COBDirection direction) const -> std::string  {
    switch (direction) {
        case hardware::COBDirection::Left: return "Left";
        case hardware::COBDirection::Right: return "Right";
        case hardware::COBDirection::Down: return "Down";
        case hardware::COBDirection::Up: return "Up";
        default: return "Unknown";
    }
}

auto COBGroup::info() const -> std::Tuple<std::usize, std::usize, std::usize> {
    auto not_used{0}, monopolized{0}, mixed{0};
    for (const auto& [_, array]: this->_groups) {
        for (const auto& group: array) {
            auto reuse = group.reuse_number();
            auto nonreuse = group.nonreuse_number();

            if (!reuse && !nonreuse) {
                not_used++;
            }
            else if (reuse && nonreuse) {
                mixed++;
            }
            else {
                monopolized++;
            }
        }
    }
    return std::Tuple<std::usize, std::usize, std::usize>{
        not_used, monopolized, mixed
    };
}

auto GlobalCOBGroups::cob_group(const hardware::COBCoord& coord) -> COBGroup& {
    return this->_cob_groups.emplace(coord, COBGroup{}).first->second;
}

auto GlobalCOBGroups::record_cob(const hardware::COBConnector& connector, bool reuse_type) -> void {
    auto& group = this->cob_group(connector.coord());
    group.record_cobgroup(connector, reuse_type);
}

auto GlobalCOBGroups::show() const -> void {
    debug::debug("COB groups:");
    for (auto& [coord, group]: this->_cob_groups) {
        debug::debug_fmt("{}:\n {}", coord, group.to_string());
    }
    debug::debug("\n");
}

auto GlobalCOBGroups::info() const -> std::Tuple<std::usize, std::usize, std::usize> {
    auto not_used{0}, monopolized{0}, mixed{0};
    for (const auto& [coord, cob]: this->_cob_groups) {
        auto [cob_not_used, cob_monopolized, cob_mixed] = cob.info();
        not_used += cob_not_used;
        monopolized += cob_monopolized;
        mixed += cob_mixed;
    }
    return std::Tuple<std::usize, std::usize, std::usize>{
        not_used, monopolized, mixed
    };
}

}

namespace std {

    std::size_t hash<kiwi::hardware::COBDirection>::operator() (const kiwi::hardware::COBDirection& dir) const noexcept {
        // Use underlying integer value of the enum for hashing
        return static_cast<std::size_t>(dir);
    }

}

