#pragma once

#include <cstddef>
#include <set>
#include <std/collection.hh>
#include <std/utility.hh>

namespace PR_tool {

auto cobunit_to_tracks(std::size_t cob_unit) -> std::Vector<std::size_t>;
auto track_to_jk(std::size_t track) -> std::Pair<std::size_t, std::size_t>;
auto tnet_allowed_jk(const std::Vector<std::size_t>& fixed_cobunits) -> std::set<std::Pair<std::size_t, std::size_t>>;

} // namespace PR_tool
