#include <hardware/interposer.hh>
#include <hardware/track/track.hh>
#include <debug/debug.hh>

#include "./utilty.hh"

#include <algorithm>
#include <filesystem>
#include <random>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

// Test-only access for key-generation internals.
#define private public
#include <algo/router/multi_mode/occupancy_view.hh>
#undef private

using namespace kiwi;
using namespace kiwi::hardware;
using namespace kiwi::algo;

static auto current_executable_dir() -> std::filesystem::path {
#if defined(__APPLE__)
    std::uint32_t buf_size = 0;
    _NSGetExecutablePath(nullptr, &buf_size);
    auto path_buf = std::string(buf_size, '\0');
    auto* raw = path_buf.data();
    if (_NSGetExecutablePath(raw, &buf_size) == 0) {
        return std::filesystem::weakly_canonical(std::filesystem::path(raw)).parent_path();
    }
    return std::filesystem::current_path();
#elif defined(__linux__)
    auto path_buf = std::vector<char>(4096, '\0');
    auto n = ::readlink("/proc/self/exe", path_buf.data(), path_buf.size() - 1);
    if (n > 0) {
        path_buf[static_cast<std::size_t>(n)] = '\0';
        return std::filesystem::weakly_canonical(std::filesystem::path(path_buf.data())).parent_path();
    }
    return std::filesystem::current_path();
#else
    return std::filesystem::current_path();
#endif
}

static auto cob_signature(const COBConnector& c) -> std::string {
    auto coord = c.coord();
    auto oss = std::ostringstream{};
    oss << "cob(" << coord.row << "," << coord.col << ")"
        << " from_dir=" << static_cast<int>(c.from_dir())
        << " to_dir=" << static_cast<int>(c.to_dir())
        << " from_idx=" << c.from_track_index()
        << " to_idx=" << c.to_track_index();
    return oss.str();
}

static auto tob_signature(const TOBConnector& c) -> std::string {
    auto oss = std::ostringstream{};
    oss << "tob_connector"
        << " bump=" << c.bump_index()
        << " hori=" << c.hori_index()
        << " vert=" << c.vert_index()
        << " track=" << c.track_index()
        << " sig_dir=" << static_cast<int>(c.single_direction());
    return oss.str();
}

static auto find_sample_cob_connectors(Interposer& interposer) -> std::Vector<COBConnector> {
    auto samples = std::Vector<COBConnector>{};
    auto seeds = std::Vector<TrackCoord>{
        TrackCoord{0, 0, TrackDirection::Vertical, 0},
        TrackCoord{0, 0, TrackDirection::Vertical, 45},
        TrackCoord{1, 1, TrackDirection::Horizontal, 10},
        TrackCoord{3, 5, TrackDirection::Vertical, 17},
        TrackCoord{5, 11, TrackDirection::Horizontal, 39},
    };

    for (auto seed : seeds) {
        auto track_opt = interposer.get_track(seed);
        if (!track_opt.has_value()) {
            continue;
        }
        auto neighbors = interposer.adjacent_tracks(track_opt.value());
        for (auto [next_track, connector] : neighbors) {
            (void)next_track;
            samples.emplace_back(connector);
            if (samples.size() >= 24) {
                return samples;
            }
        }
    }
    return samples;
}

static auto find_sample_tob_connectors(Interposer& interposer) -> std::Vector<TOBConnector> {
    auto samples = std::Vector<TOBConnector>{};

    // Randomly generate unique bump seeds in legal TOB/bump index ranges.
    auto rng = std::mt19937{std::random_device{}()};
    auto row_dist = std::uniform_int_distribution<int>{0, Interposer::TOB_ARRAY_HEIGHT - 1};
    auto col_dist = std::uniform_int_distribution<int>{0, Interposer::TOB_ARRAY_WIDTH - 1};
    auto bump_dist = std::uniform_int_distribution<int>{0, TOB::INDEX_SIZE - 1};

    auto bump_seeds = std::Vector<std::Tuple<int, int, int>>{};
    auto bump_seed_keys = std::unordered_set<std::string>{};
    constexpr int max_seed_attempts = 512;
    constexpr std::usize max_seed_count = 64;

    for (int attempt = 0; attempt < max_seed_attempts && bump_seeds.size() < max_seed_count; ++attempt) {
        const auto r = row_dist(rng);
        const auto c = col_dist(rng);
        const auto b = bump_dist(rng);
        const auto key = std::to_string(r) + "," + std::to_string(c) + "," + std::to_string(b);
        if (bump_seed_keys.insert(key).second) {
            bump_seeds.emplace_back(r, c, b);
        }
    }

    // Enforce resource uniqueness across sampled connectors to mimic exclusive usage.
    auto used_bump_index = std::unordered_set<std::usize>{};
    auto used_hori_index = std::unordered_set<std::usize>{};
    auto used_vert_index = std::unordered_set<std::usize>{};
    auto used_track_index = std::unordered_set<std::usize>{};

    for (auto [r, c, b] : bump_seeds) {
        auto bump_opt = interposer.get_bump(r, c, b);
        if (!bump_opt.has_value()) {
            continue;
        }
        auto available = interposer.available_tracks_bump_to_track(bump_opt.value());
        for (auto [track, connector] : available) {
            (void)track;

            const auto bump_index = connector.bump_index();
            const auto hori_index = connector.hori_index();
            const auto vert_index = connector.vert_index();
            const auto track_index = connector.track_index();

            if (used_bump_index.contains(bump_index)
                || used_hori_index.contains(hori_index)
                || used_vert_index.contains(vert_index)
                || used_track_index.contains(track_index)) {
                continue;
            }

            used_bump_index.emplace(bump_index);
            used_hori_index.emplace(hori_index);
            used_vert_index.emplace(vert_index);
            used_track_index.emplace(track_index);
            samples.emplace_back(connector);

            if (samples.size() >= 24) {
                return samples;
            }
        }
    }
    return samples;
}

static void test_occupancy_view_key_generation_and_recording() {
    debug::debug("test_occupancy_view_key_generation_and_recording");

    auto interposer = Interposer{};
    auto view = OccupancyView{&interposer};

    auto cob_samples = find_sample_cob_connectors(interposer);
    auto tob_samples = find_sample_tob_connectors(interposer);

    ASSERT(cob_samples.size() >= 2);
    ASSERT(tob_samples.size() >= 2);

    debug::info("[OccupancyView][COB] ==== key generation check ====");
    auto cob_key_to_sig = std::unordered_map<std::u64, std::string>{};
    auto cob_key_collision = false;

    for (std::usize i = 0; i < cob_samples.size(); ++i) {
        const auto& c = cob_samples[i];
        auto key = view.cob_key(c);
        auto sig = cob_signature(c);

        debug::info_fmt("COB sample#{} key={} {}", i, key, sig);

        auto it = cob_key_to_sig.find(key);
        if (it != cob_key_to_sig.end() && it->second != sig) {
            cob_key_collision = true;
            debug::info("  COLLISION: same key maps to two connectors");
            debug::info_fmt("    first:  {}", it->second);
            debug::info_fmt("    second: {}", sig);
        } else {
            cob_key_to_sig.emplace(key, sig);
        }
    }

    debug::info_fmt("[OccupancyView][COB] collision_detected={}", cob_key_collision ? "YES" : "NO");

    // COB occupancy record behavior and per-mode isolation.
    auto cob_a = cob_samples[0];
    auto cob_b = cob_samples[1];
    auto mode1 = 1;
    auto mode2 = 2;

    ASSERT(!view.is_cobconnector_occupied(mode1, cob_a));
    ASSERT(!view.is_cobconnector_occupied(mode2, cob_a));

    view.occupy_cobconnector(mode1, cob_a, true);
    ASSERT(view.is_cobconnector_occupied(mode1, cob_a));
    ASSERT(!view.is_cobconnector_occupied(mode2, cob_a));

    view.occupy_cobconnector(mode1, cob_b, false);
    ASSERT(view.is_cobconnector_occupied(mode1, cob_b));
    view.clear_mode(mode1);

    // locked cob_a should remain after clear_mode, cob_b should be cleared.
    ASSERT(view.is_cobconnector_occupied(mode1, cob_a));
    ASSERT(!view.is_cobconnector_occupied(mode1, cob_b));

    debug::info("[OccupancyView][COB] mode clear check passed");

    debug::info("[OccupancyView][TOB] ==== key generation check ====");
    auto tob_exact_collision = false;
    auto tob_overlap_pairs = std::usize{0};
    auto tob_keyset_to_sig = std::unordered_map<std::string, std::string>{};

    auto keyset_string = [](const std::HashSet<std::u64>& keyset) {
        auto vec = std::vector<std::u64>{};
        vec.reserve(keyset.size());
        for (auto k : keyset) {
            vec.emplace_back(k);
        }
        std::sort(vec.begin(), vec.end());
        auto oss = std::ostringstream{};
        for (auto k : vec) {
            oss << k << ";";
        }
        return oss.str();
    };

    for (std::usize i = 0; i < tob_samples.size(); ++i) {
        const auto& c = tob_samples[i];
        auto keys = view.tob_key(c);
        auto sig = tob_signature(c);
        auto ks = keyset_string(keys);

        debug::info_fmt("TOB sample#{} key_count={} {}", i, keys.size(), sig);
        debug::info_fmt("  keys={}", ks);

        auto it = tob_keyset_to_sig.find(ks);
        if (it != tob_keyset_to_sig.end() && it->second != sig) {
            tob_exact_collision = true;
            debug::info("  EXACT-SET COLLISION: two different connectors have identical key set");
            debug::info_fmt("    first:  {}", it->second);
            debug::info_fmt("    second: {}", sig);
        } else {
            tob_keyset_to_sig.emplace(ks, sig);
        }
    }

    // Pair-wise key overlap (shared TOB mux register addresses).
    for (std::usize i = 0; i < tob_samples.size(); ++i) {
        auto ki = view.tob_key(tob_samples[i]);
        for (std::usize j = i + 1; j < tob_samples.size(); ++j) {
            auto kj = view.tob_key(tob_samples[j]);
            auto overlap = false;
            for (auto k : ki) {
                if (kj.contains(k)) {
                    overlap = true;
                    break;
                }
            }
            if (overlap) {
                tob_overlap_pairs += 1;
            }
        }
    }

    debug::info_fmt("[OccupancyView][TOB] exact_set_collision_detected={}", tob_exact_collision ? "YES" : "NO");
    debug::info_fmt("[OccupancyView][TOB] overlap_pair_count={}", tob_overlap_pairs);

    // TOB occupancy record behavior and per-mode isolation.
    auto tob_a = tob_samples[0];
    auto tob_b = tob_samples[1];

    ASSERT(!view.is_tobconnector_occupied(mode1, tob_a));
    ASSERT(!view.is_tobconnector_occupied(mode2, tob_a));

    view.occupy_tobconnector(mode1, tob_a, true);
    ASSERT(view.is_tobconnector_occupied(mode1, tob_a));
    ASSERT(!view.is_tobconnector_occupied(mode2, tob_a));

    view.occupy_tobconnector(mode1, tob_b, false);
    auto tob_b_before_clear = view.is_tobconnector_occupied(mode1, tob_b);
    debug::info_fmt("[OccupancyView][TOB] second connector occupied before clear={}", tob_b_before_clear ? "YES" : "NO");

    view.clear_mode(mode1);

    ASSERT(view.is_tobconnector_occupied(mode1, tob_a));
    auto tob_b_after_clear = view.is_tobconnector_occupied(mode1, tob_b);
    debug::info_fmt("[OccupancyView][TOB] second connector occupied after clear={}", tob_b_after_clear ? "YES" : "NO");

    debug::info("[OccupancyView] key and record checks completed");
}

auto test_occupancyview_main() -> void {
    test_occupancy_view_key_generation_and_recording();
}