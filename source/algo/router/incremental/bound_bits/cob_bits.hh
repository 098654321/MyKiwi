#pragma once

#include <std/collection.hh>
#include <hardware/cob/cob.hh>
#include <hardware/cob/cobdirection.hh>
#include "./bits_group.hh"





namespace PR_tool::algo {

inline constexpr std::usize COBGroupSize = 32;
inline constexpr std::usize COBGroupNum = 4;

class COBGroup {
public:
    COBGroup() : _groups{} {}
    ~COBGroup() = default;

public:
    auto record_cobgroup(const hardware::COBConnector& connector, bool reuse_type) -> void;
    auto trackindex_to_groupinfo(std::usize track_index) const -> std::Tuple<std::usize, std::usize>;

    auto to_string() const -> std::String;
    auto cobdirection_to_string(hardware::COBDirection direction) const -> std::string;
    auto info() const -> std::Tuple<std::usize, std::usize>;

private:
    std::HashMap<hardware::COBDirection, std::Array<BitsGroup<COBGroupSize>, COBGroupNum>> _groups;
};

class GlobalCOBGroups {
public:
    GlobalCOBGroups() : _cob_groups{} {}
    ~GlobalCOBGroups() = default;

public:
    auto cob_group(const hardware::COBCoord& coord) -> COBGroup&;
    auto record_cob(const hardware::COBConnector& connector, bool reuse_type) -> void;

    auto show() const -> void;
    auto info() const -> std::Tuple<std::usize, std::usize>;

private:
    std::HashMap<hardware::COBCoord, COBGroup> _cob_groups;
};

}


