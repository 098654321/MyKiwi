#include "./hungarian.hh"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace kiwi::algo::multi_mode {

    auto hungarian_max_weight(const std::Vector<std::Vector<std::i64>>& weights) -> std::Vector<std::usize> {
        const auto n = weights.size();
        if (n == 0) {
            return {};
        }
        for (const auto& row : weights) {
            if (row.size() != n) {
                throw std::runtime_error("hungarian_max_weight: weights must be square");
            }
        }

        std::i64 max_w = 0;
        for (const auto& row : weights) {
            for (auto w : row) {
                if (w < 0) {
                    throw std::runtime_error("hungarian_max_weight: weights must be non-negative");
                }
                max_w = std::max(max_w, w);
            }
        }

        // Hungarian algorithm for min-cost assignment (1-indexed internals).
        // Convert max-weight to min-cost by cost = max_w - weight.
        auto u = std::Vector<std::i64>(n + 1, 0);
        auto v = std::Vector<std::i64>(n + 1, 0);
        auto p = std::Vector<std::usize>(n + 1, 0);
        auto way = std::Vector<std::usize>(n + 1, 0);

        for (std::usize i = 1; i <= n; ++i) {
            p[0] = i;
            auto j0 = std::usize{0};
            auto minv = std::Vector<std::i64>(n + 1, std::numeric_limits<std::i64>::max());
            auto used = std::Vector<bool>(n + 1, false);

            do {
                used[j0] = true;
                auto i0 = p[j0];
                auto delta = std::numeric_limits<std::i64>::max();
                auto j1 = std::usize{0};

                for (std::usize j = 1; j <= n; ++j) {
                    if (used[j]) {
                        continue;
                    }
                    const auto cost = (max_w - weights[i0 - 1][j - 1]) - u[i0] - v[j];
                    if (cost < minv[j]) {
                        minv[j] = cost;
                        way[j] = j0;
                    }
                    if (minv[j] < delta) {
                        delta = minv[j];
                        j1 = j;
                    }
                }

                for (std::usize j = 0; j <= n; ++j) {
                    if (used[j]) {
                        u[p[j]] += delta;
                        v[j] -= delta;
                    } else {
                        minv[j] -= delta;
                    }
                }

                j0 = j1;
            } while (p[j0] != 0);

            // augmenting
            do {
                auto j1 = way[j0];
                p[j0] = p[j1];
                j0 = j1;
            } while (j0 != 0);
        }

        auto assign = std::Vector<std::usize>(n, 0);
        for (std::usize j = 1; j <= n; ++j) {
            if (p[j] == 0) {
                continue;
            }
            assign[p[j] - 1] = j - 1;
        }
        return assign;
    }

}

