#include "ilp_speedup.hh"

namespace PR_tool {

auto collect_net_bumps(const std::Vector<Net_cost_record>& records) -> std::set<Bump_coord> {
    auto bumps = std::set<Bump_coord> {};
    for (const auto& record : records) {
        for (const auto& b : record.start_bumps) {
            bumps.insert(b);
        }
        for (const auto& b : record.end_bumps) {
            bumps.insert(b);
        }
    }
    return bumps;
}

auto cobunit_to_tracks(const std::size_t cob_unit) -> std::Vector<std::size_t> {
    const auto bank = cob_unit < 8 ? 0UL : 1UL;
    const auto unit = cob_unit - (8UL * bank);
    auto tracks = std::Vector<std::size_t> {};
    tracks.reserve(8);
    for (std::size_t g = 0; g < 8; ++g) {
        tracks.push_back((8 * g) + unit + (bank * 64));
    }
    return tracks;
}

auto track_to_jk(const std::size_t track) -> std::Pair<std::size_t, std::size_t> {
    const auto v = track % 64;
    return {v / 8, v % 8};
}

auto tnet_allowed_jk(const std::Vector<std::size_t>& fixed_cobunits) -> std::set<std::Pair<std::size_t, std::size_t>> {
    auto allowed = std::set<std::Pair<std::size_t, std::size_t>> {};
    for (const auto c : fixed_cobunits) {
        for (const auto track : cobunit_to_tracks(c)) {
            allowed.insert(track_to_jk(track));
        }
    }
    return allowed;
}

} // namespace PR_tool
