#include "./utilty.hh"

#include <circuit/net/types/bbnet.hh>
#include <circuit/net/types/btnet.hh>
#include <circuit/net/types/tbnet.hh>
#include <circuit/net/types/syncnet.hh>
#include <hardware/interposer.hh>
#include <debug/debug.hh>
#include <global/debug/console.hh>
#include <std/memory.hh>
#include <random>

using namespace kiwi;
using namespace kiwi::hardware;
using namespace kiwi::circuit;

static auto dump_bbox_case(
    std::StringView case_id,
    std::StringView type,
    const std::String& net_name,
    std::String endpoint_desc,
    const BoundingBox& bbox
) -> void {
    kiwi::console::println_fmt("----------------------------------------");
    kiwi::console::println_fmt("case={} type={} name={}", case_id, type, net_name);
    kiwi::console::println_fmt("endpoints={}", endpoint_desc);
    kiwi::console::println_fmt(
        "mode={} bbox=[row:{}..{}, col:{}..{}]",
        bbox.mode,
        bbox.region.row_min,
        bbox.region.row_max,
        bbox.region.col_min,
        bbox.region.col_max
    );
}

static auto bump_desc(Bump* begin, Bump* end) -> std::String {
    const auto c1 = begin->tob()->coord();
    const auto c2 = end->tob()->coord();
    return "begin_bump=(tob:" + std::to_string(c1.row) + "," + std::to_string(c1.col) +
        " idx:" + std::to_string(begin->index()) + ") end_bump=(tob:" +
        std::to_string(c2.row) + "," + std::to_string(c2.col) + " idx:" +
        std::to_string(end->index()) + ")";
}

static auto bump_track_desc(Bump* begin, Track* end) -> std::String {
    const auto c = begin->tob()->coord();
    const auto tc = end->coord();
    auto dir = std::String{tc.dir == TrackDirection::Horizontal ? "H" : "V"};
    return "begin_bump=(tob:" + std::to_string(c.row) + "," + std::to_string(c.col) +
        " idx:" + std::to_string(begin->index()) + ") end_track=(row:" +
        std::to_string(tc.row) + " col:" + std::to_string(tc.col) + " dir:" +
        dir + " idx:" + std::to_string(tc.index) + ")";
}

static auto track_bump_desc(Track* begin, Bump* end) -> std::String {
    const auto tc = begin->coord();
    const auto c = end->tob()->coord();
    auto dir = std::String{tc.dir == TrackDirection::Horizontal ? "H" : "V"};
    return "begin_track=(row:" + std::to_string(tc.row) + " col:" + std::to_string(tc.col) +
        " dir:" + dir + " idx:" + std::to_string(tc.index) + ") end_bump=(tob:" +
        std::to_string(c.row) + "," + std::to_string(c.col) + " idx:" +
        std::to_string(end->index()) + ")";
}

static auto random_bbox(std::mt19937& rng, int mode) -> BoundingBox {
    auto row_dist = std::uniform_int_distribution<int>{0, Interposer::COB_ARRAY_HEIGHT + 1};
    auto col_dist = std::uniform_int_distribution<int>{0, Interposer::COB_ARRAY_WIDTH + 1};

    auto r1 = static_cast<std::i64>(row_dist(rng));
    auto r2 = static_cast<std::i64>(row_dist(rng));
    auto c1 = static_cast<std::i64>(col_dist(rng));
    auto c2 = static_cast<std::i64>(col_dist(rng));

    auto region = Region{};
    region.row_min = std::min(r1, r2);
    region.row_max = std::max(r1, r2);
    region.col_min = std::min(c1, c2);
    region.col_max = std::max(c1, c2);

    return BoundingBox{region, mode, nullptr, nullptr, std::nullopt};
}

static void test_bbox_overlap_random() {
    auto rng = std::mt19937{std::random_device{}()};
    auto b1 = random_bbox(rng, 1);
    auto b2 = random_bbox(rng, 2);

    // Same overlap logic as route_multi_mode.cc
    auto overlap = [](const Region& a, const Region& b) -> std::Option<Region> {
        auto r = Region{};
        r.row_min = std::max(a.row_min, b.row_min);
        r.row_max = std::min(a.row_max, b.row_max);
        r.col_min = std::max(a.col_min, b.col_min);
        r.col_max = std::min(a.col_max, b.col_max);
        if (r.row_min > r.row_max || r.col_min > r.col_max) {
            return std::nullopt;
        }
        return r;
    };

    auto ov = overlap(b1.region, b2.region);

    kiwi::console::println_fmt("----------------------------------------");
    kiwi::console::println("case=ov-1 type=RandomBBoxOverlap");
    kiwi::console::println_fmt(
        "bbox_a(mode={}): row:{}..{} col:{}..{}",
        b1.mode, b1.region.row_min, b1.region.row_max, b1.region.col_min, b1.region.col_max
    );
    kiwi::console::println_fmt(
        "bbox_b(mode={}): row:{}..{} col:{}..{}",
        b2.mode, b2.region.row_min, b2.region.row_max, b2.region.col_min, b2.region.col_max
    );

    if (ov.has_value()) {
        kiwi::console::println_fmt(
            "overlap: row:{}..{} col:{}..{}",
            ov->row_min, ov->row_max, ov->col_min, ov->col_max
        );
    } else {
        kiwi::console::println("overlap: nullopt");
    }
}

static void test_bbox_bump_to_bump() {
    auto interposer = Interposer{};
    const auto modes = std::HashSet<int>{1};

    {
        auto* begin_bump = interposer.get_bump(0, 0, 45).value();
        auto* end_bump = interposer.get_bump(0, 2, 4).value();
        auto name = std::String{"bbox_bb_same_row"};
        auto net = BumpToBumpNet{begin_bump, end_bump, modes, name};

        auto bbox = net.compute_bounding_box(1);
        ASSERT(bbox.has_value());
        ASSERT(bbox->net == &net);
        dump_bbox_case("bb-1", "BumpToBumpNet", net.name(), bump_desc(begin_bump, end_bump), bbox.value());
    }

    {
        auto* begin_bump = interposer.get_bump(3, 1, 34).value();
        auto* end_bump = interposer.get_bump(1, 3, 78).value();
        auto name = std::String{"bbox_bb_cross_row"};
        auto net = BumpToBumpNet{begin_bump, end_bump, modes, name};

        auto bbox = net.compute_bounding_box(1);
        ASSERT(bbox.has_value());
        ASSERT(bbox->net == &net);
        dump_bbox_case("bb-2", "BumpToBumpNet", net.name(), bump_desc(begin_bump, end_bump), bbox.value());
    }
}

static void test_bbox_bump_to_track() {
    auto interposer = Interposer{};
    const auto modes = std::HashSet<int>{1};

    {
        auto* begin_bump = interposer.get_bump(2, 1, 12).value();
        auto* end_track = interposer.get_track(4, 0, TrackDirection::Horizontal, 0).value();
        auto name = std::String{"bbox_bt_left_io"};
        auto net = BumpToTrackNet{begin_bump, end_track, modes, name};

        auto bbox = net.compute_bounding_box(1);
        ASSERT(bbox.has_value());
        ASSERT(bbox->net == &net);
        dump_bbox_case("bt-1", "BumpToTrackNet", net.name(), bump_track_desc(begin_bump, end_track), bbox.value());
    }

    {
        auto* begin_bump = interposer.get_bump(1, 0, 9).value();
        auto* end_track = interposer.get_track(9, 2, TrackDirection::Vertical, 39).value();
        auto name = std::String{"bbox_bt_down_io"};
        auto net = BumpToTrackNet{begin_bump, end_track, modes, name};

        auto bbox = net.compute_bounding_box(1);
        ASSERT(bbox.has_value());
        ASSERT(bbox->net == &net);
        dump_bbox_case("bt-2", "BumpToTrackNet", net.name(), bump_track_desc(begin_bump, end_track), bbox.value());
    }
}

static void test_bbox_track_to_bump() {
    auto interposer = Interposer{};
    const auto modes = std::HashSet<int>{2};

    {
        auto* begin_track = interposer.get_track(0, 5, TrackDirection::Vertical, 45).value();
        auto* end_bump = interposer.get_bump(2, 2, 90).value();
        auto name = std::String{"bbox_tb_up_io"};
        auto net = TrackToBumpNet{begin_track, end_bump, modes, name};

        auto bbox = net.compute_bounding_box(2);
        ASSERT(bbox.has_value());
        ASSERT(bbox->net == &net);
        dump_bbox_case("tb-1", "TrackToBumpNet", net.name(), track_bump_desc(begin_track, end_bump), bbox.value());
    }

    {
        auto* begin_track = interposer.get_track(6, 12, TrackDirection::Horizontal, 18).value();
        auto* end_bump = interposer.get_bump(0, 1, 23).value();
        auto name = std::String{"bbox_tb_right_io"};
        auto net = TrackToBumpNet{begin_track, end_bump, modes, name};

        auto bbox = net.compute_bounding_box(2);
        ASSERT(bbox.has_value());
        ASSERT(bbox->net == &net);
        dump_bbox_case("tb-2", "TrackToBumpNet", net.name(), track_bump_desc(begin_track, end_bump), bbox.value());
    }
}

static void test_bbox_syncnet() {
    auto interposer = Interposer{};
    const auto modes = std::HashSet<int>{1};

    {
        auto name_sub1 = std::String{"sync_a_sub_1"};
        auto name_sub2 = std::String{"sync_a_sub_2"};
        auto name_sync = std::String{"bbox_sync_case_a"};

        // Requirement: all starts on the same topdie, all ends on the same topdie.
        auto sub1 = std::make_shared<BumpToBumpNet>(
            interposer.get_bump(0, 0, 1).value(),
            interposer.get_bump(2, 3, 2).value(),
            modes,
            name_sub1
        );
        auto sub2 = std::make_shared<BumpToBumpNet>(
            interposer.get_bump(0, 0, 15).value(),
            interposer.get_bump(2, 3, 23).value(),
            modes,
            name_sub2
        );

        auto sync = SyncNet{
            std::Vector<std::Rc<BumpToBumpNet>>{sub1, sub2},
            std::Vector<std::Rc<BumpToTrackNet>>{},
            std::Vector<std::Rc<TrackToBumpNet>>{},
            modes,
            name_sync
        };

        auto bbox = sync.compute_bounding_box(1);
        ASSERT(bbox.has_value());
        ASSERT(bbox->net == &sync);
        dump_bbox_case(
            "sync-1",
            "SyncNet",
            sync.name(),
            "all_subnet_starts=tob(0,0), all_subnet_ends=tob(2,3)",
            bbox.value()
        );
    }

    {
        auto name_sub1 = std::String{"sync_b_sub_1"};
        auto name_sub2 = std::String{"sync_b_sub_2"};
        auto name_sync = std::String{"bbox_sync_case_b"};

        // Requirement: all starts on the same topdie, all ends on the same topdie.
        auto sub1 = std::make_shared<BumpToBumpNet>(
            interposer.get_bump(3, 1, 34).value(),
            interposer.get_bump(1, 0, 8).value(),
            modes,
            name_sub1
        );
        auto sub2 = std::make_shared<BumpToBumpNet>(
            interposer.get_bump(3, 1, 77).value(),
            interposer.get_bump(1, 0, 95).value(),
            modes,
            name_sub2
        );

        auto sync = SyncNet{
            std::Vector<std::Rc<BumpToBumpNet>>{sub1, sub2},
            std::Vector<std::Rc<BumpToTrackNet>>{},
            std::Vector<std::Rc<TrackToBumpNet>>{},
            modes,
            name_sync
        };

        auto bbox = sync.compute_bounding_box(1);
        ASSERT(bbox.has_value());
        ASSERT(bbox->net == &sync);
        dump_bbox_case(
            "sync-2",
            "SyncNet",
            sync.name(),
            "all_subnet_starts=tob(3,1), all_subnet_ends=tob(1,0)",
            bbox.value()
        );
    }
}

void test_bbox_main() {
    kiwi::console::println("=== test_bbox_main: compute_bounding_box sanity ===");
    test_bbox_bump_to_bump();
    test_bbox_bump_to_track();
    test_bbox_track_to_bump();
    test_bbox_syncnet();
    test_bbox_overlap_random();
    kiwi::console::println("=== test_bbox_main done ===");
}
