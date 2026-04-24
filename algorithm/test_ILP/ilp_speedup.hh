#pragma once

#include "ilp_types.hh"

#include <set>
#include <std/collection.hh>
#include <std/utility.hh>

namespace PR_tool {

auto collect_net_bumps(const std::Vector<Net_cost_record>& records) -> std::set<Bump_coord>;
auto cobunit_to_tracks(std::size_t cob_unit) -> std::Vector<std::size_t>;
auto track_to_jk(std::size_t track) -> std::Pair<std::size_t, std::size_t>;
auto tnet_allowed_jk(const std::Vector<std::size_t>& fixed_cobunits) -> std::set<std::Pair<std::size_t, std::size_t>>;

} // namespace PR_tool
