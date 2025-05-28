#include "./tob_bits.hh"
#include <ranges>


namespace kiwi::algo {

auto TOBGroup::bump_to_hori_info(std::usize index) const -> std::Tuple<std::usize, std::usize> {
    return {
        index / hardware::TOB::BUMP_TO_HORI_MUX_SIZE,
        index % hardware::TOB::BUMP_TO_HORI_MUX_SIZE,  
    };
}

auto TOBGroup::hori_to_vert_info(std::usize index) const -> std::Tuple<std::usize, std::usize> {
    if (index >= (hardware::TOB::INDEX_SIZE / 2)) {
        return {
            index % hardware::TOB::HORI_TO_VERI_MUX_SIZE + (hardware::TOB::HORI_TO_VERI_MUX_COUNT / 2),
            (index - hardware::TOB::INDEX_SIZE / 2) / hardware::TOB::HORI_TO_VERI_MUX_SIZE
        };
    } else {
        return {
            index % hardware::TOB::HORI_TO_VERI_MUX_SIZE,
            index / hardware::TOB::HORI_TO_VERI_MUX_SIZE, 
        };
    }
}

auto TOBGroup::vert_to_track_info(std::usize index) const -> std::Tuple<std::usize, std::usize> {
    if (index >= (hardware::TOB::INDEX_SIZE / 2)) {
        return {
            index - (hardware::TOB::INDEX_SIZE / 2),
            1
        };
    } else {
        return {
            index,
            0
        };
    }
}

auto TOBGroup::record_tobmux(const hardware::TOBConnector& connector, bool reuse_type) -> void {
    auto [bump_mux, bump_mux_input] = this->bump_to_hori_info(connector.bump_index());
    auto& bmux = this->_bump_groups.at(bump_mux);
    reuse_type ? bmux.add_reuse_number() : bmux.add_nonreuse_number();

    auto [hori_mux, hori_mux_input] = this->hori_to_vert_info(connector.hori_index());
    auto& hmux = this->_hori_groups.at(hori_mux);
    reuse_type ? hmux.add_reuse_number() : hmux.add_nonreuse_number();

    auto [vert_mux, vert_mux_input] = this->vert_to_track_info(connector.vert_index());
    auto& vmux = this->_vert_groups.at(vert_mux/VertMuxSize);
    reuse_type ? vmux.add_reuse_number() : vmux.add_nonreuse_number();
}

auto TOBGroup::to_string() const -> std::String {
    std::String message {};

    message += "bump to hori mux:";
    for (auto i: std::views::iota(0, (int)BumpMuxNum)) {
        auto group = this->_bump_groups.at(i);
        message += std::format("group {}, {}\n", i, group.to_string());
    }

    message += "hori to vert mux:";
    for (auto i: std::views::iota(0, (int)HoriMuxNum)) {
        auto group = this->_hori_groups.at(i);
        message += std::format("group {}, {}\n", i, group.to_string());
    }

    message += "vert to track mux:";
    for (auto i: std::views::iota(0, (int)VertMuxNum)) {
        auto group = this->_vert_groups.at(i);
        message += std::format("group {}, {}\n", i, group.to_string());
    }
    
    return message;
}

auto TOBGroup::info() -> std::Tuple<std::usize, std::usize, std::usize> {
    auto not_used{0}, monopolized{0}, mixed{0};
    auto collect = [&](const auto& array) {
        for (auto& group: array) {
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
    };

    collect(this->_bump_groups);
    collect(this->_hori_groups);
    collect(this->_vert_groups);
    
    return std::Tuple<std::usize, std::usize, std::usize>{
        not_used, monopolized, mixed
    };
}

auto GlobalTOBGroup::tob_group(const hardware::TOBCoord& coord) -> TOBGroup& {
    return this->_tob_groups.emplace(coord, TOBGroup{}).first->second;
}

auto GlobalTOBGroup::record_tob(const hardware::TOBCoord& coord, const hardware::TOBConnector& connector, bool reuse_type) -> void {
    auto& group = this->tob_group(coord);
    group.record_tobmux(connector, reuse_type);
}

auto GlobalTOBGroup::show() const -> void {
    debug::debug("TOB groups:");
    for (auto& [coord, group]: this->_tob_groups) {
        debug::debug_fmt("{}:\n {}", coord, group.to_string());
    }
    debug::debug("\n");
}

auto GlobalTOBGroup::info() -> std::Tuple<std::usize, std::usize, std::usize> {
    auto not_used{0}, monopolized{0}, mixed{0};
    for (auto& [coord, group]: this->_tob_groups) {
        auto [group_not_used, group_mono, group_mixed] = group.info();
        not_used += group_not_used;
        monopolized += group_mono;
        mixed += group_mixed;
    }

    return std::Tuple<std::usize, std::usize, std::usize>{
        not_used, monopolized, mixed
    };
}

}

