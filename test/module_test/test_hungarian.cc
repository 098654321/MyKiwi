#include "./utilty.hh"

#include <algo/router/multi_mode/hungarian.hh>
#include <global/debug/console.hh>
#include <std/collection.hh>
#include <std/integer.hh>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>

using namespace kiwi;

namespace {

using WeightMatrix = std::Vector<std::Vector<std::i64>>;

struct BruteForceResult {
    std::i64 best_total;
    std::Vector<std::usize> assignment;
};

static auto calc_total_weight(
    const WeightMatrix& weights,
    const std::Vector<std::usize>& assign
) -> std::i64 {
    auto total = std::i64{0};
    for (std::usize i = 0; i < assign.size(); ++i) {
        total += weights[i][assign[i]];
    }
    return total;
}

static auto is_valid_assignment(std::usize n, const std::Vector<std::usize>& assign) -> bool {
    if (assign.size() != n) {
        return false;
    }

    auto used = std::Vector<bool>(n, false);
    for (auto col : assign) {
        if (col >= n) {
            return false;
        }
        if (used[col]) {
            return false;
        }
        used[col] = true;
    }
    return true;
}

static auto brute_force_best(const WeightMatrix& weights) -> BruteForceResult {
    const auto n = weights.size();
    auto perm = std::Vector<std::usize>(n);
    std::iota(perm.begin(), perm.end(), std::usize{0});

    auto best_total = std::numeric_limits<std::i64>::min();
    auto best_assign = std::Vector<std::usize>(n, 0);

    do {
        auto total = std::i64{0};
        for (std::usize i = 0; i < n; ++i) {
            total += weights[i][perm[i]];
        }
        if (total > best_total) {
            best_total = total;
            best_assign = perm;
        }
    } while (std::next_permutation(perm.begin(), perm.end()));

    return BruteForceResult{best_total, best_assign};
}

static auto assignment_to_string(const std::Vector<std::usize>& assign) -> std::String {
    auto s = std::String{"["};
    for (std::usize i = 0; i < assign.size(); ++i) {
        if (i != 0) {
            s += ", ";
        }
        s += "r" + std::to_string(i) + "->c" + std::to_string(assign[i]);
    }
    s += "]";
    return s;
}

static auto print_weights(const WeightMatrix& weights) -> void {
    const auto n = weights.size();
    auto width = std::size_t{1};

    for (const auto& row : weights) {
        for (auto w : row) {
            const auto digits = std::to_string(w).size();
            width = std::max(width, digits);
        }
    }
    width = std::max<std::size_t>(width, 2);

    std::cout << "weights (" << n << "x" << n << "):\n";
    std::cout << "      ";
    for (std::usize j = 0; j < n; ++j) {
        std::cout << "c" << std::setw(static_cast<int>(width)) << j << " ";
    }
    std::cout << "\n";

    for (std::usize i = 0; i < n; ++i) {
        std::cout << "r" << std::setw(3) << i << " | ";
        for (std::usize j = 0; j < n; ++j) {
            std::cout << std::setw(static_cast<int>(width) + 2) << weights[i][j] << " ";
        }
        std::cout << "\n";
    }
}

static auto make_random_weights(std::mt19937_64& rng, std::usize n) -> WeightMatrix {
    auto dist = std::uniform_int_distribution<int>{0, 99};
    auto w = WeightMatrix{};
    w.reserve(n);

    for (std::usize i = 0; i < n; ++i) {
        auto row = std::Vector<std::i64>{};
        row.reserve(n);
        for (std::usize j = 0; j < n; ++j) {
            row.push_back(static_cast<std::i64>(dist(rng)));
        }
        w.push_back(std::move(row));
    }

    return w;
}

static auto print_case_result(
    std::usize case_id,
    std::uint64_t seed,
    const WeightMatrix& weights,
    const std::Vector<std::usize>& hungarian_assign,
    const BruteForceResult& brute
) -> void {
    std::cout << "========================================\n";
    std::cout << "case #" << case_id << "  seed=" << seed << "\n";
    print_weights(weights);

    const auto hungarian_total = calc_total_weight(weights, hungarian_assign);
    std::cout << "hungarian: " << assignment_to_string(hungarian_assign)
              << "  total=" << hungarian_total << "\n";
    std::cout << "oracle   : " << assignment_to_string(brute.assignment)
              << "  total=" << brute.best_total << "\n";
}

static void test_hungarian_basic_cases() {
    {
        auto weights = WeightMatrix{};
        auto assign = algo::hungarian_max_weight(weights);
        ASSERT_EQ(assign.size(), 0);
    }

    {
        auto weights = WeightMatrix{{42}};
        auto assign = algo::hungarian_max_weight(weights);
        ASSERT_EQ(assign.size(), 1);
        ASSERT_EQ(assign[0], 0);
        ASSERT_EQ(calc_total_weight(weights, assign), 42);
    }

    {
        auto weights = WeightMatrix{
            {9, 1, 1},
            {1, 9, 1},
            {1, 1, 9},
        };
        auto assign = algo::hungarian_max_weight(weights);
        ASSERT_EQ(assign.size(), 3);
        ASSERT_EQ(assign[0], 0);
        ASSERT_EQ(assign[1], 1);
        ASSERT_EQ(assign[2], 2);
    }

    {
        auto weights = WeightMatrix{
            {1, 9, 1},
            {1, 1, 9},
            {9, 1, 1},
        };
        auto assign = algo::hungarian_max_weight(weights);
        ASSERT_EQ(assign.size(), 3);
        ASSERT_EQ(assign[0], 1);
        ASSERT_EQ(assign[1], 2);
        ASSERT_EQ(assign[2], 0);
    }
}

static void test_hungarian_invalid_inputs() {
    {
        auto thrown = false;
        try {
            auto weights = WeightMatrix{
                {1, 2, 3},
                {4, 5},
            };
            auto assign = algo::hungarian_max_weight(weights);
            (void)assign;
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        ASSERT(thrown);
    }

    {
        auto thrown = false;
        try {
            auto weights = WeightMatrix{
                {1, 2},
                {-3, 4},
            };
            auto assign = algo::hungarian_max_weight(weights);
            (void)assign;
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        ASSERT(thrown);
    }
}

static void test_hungarian_randomized() {
    constexpr auto kCaseCount = std::usize{100};
    constexpr auto kMinSize = std::usize{2};
    constexpr auto kMaxSize = std::usize{7};

    const auto seed = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
    auto rng = std::mt19937_64{seed};
    auto n_dist = std::uniform_int_distribution<int>(
        static_cast<int>(kMinSize),
        static_cast<int>(kMaxSize)
    );

    kiwi::console::println_fmt("Hungarian randomized test seed={}", seed);

    for (std::usize case_id = 1; case_id <= kCaseCount; ++case_id) {
        const auto n = static_cast<std::usize>(n_dist(rng));
        const auto weights = make_random_weights(rng, n);

        const auto hungarian_assign = algo::hungarian_max_weight(weights);
        ASSERT(is_valid_assignment(n, hungarian_assign));

        const auto brute = brute_force_best(weights);
        const auto hungarian_total = calc_total_weight(weights, hungarian_assign);

        print_case_result(case_id, seed, weights, hungarian_assign, brute);
        ASSERT_EQ(hungarian_total, brute.best_total);
    }
}

} // namespace

void test_hungarian_main() {
    test_hungarian_basic_cases();
    test_hungarian_invalid_inputs();
    test_hungarian_randomized();
}
