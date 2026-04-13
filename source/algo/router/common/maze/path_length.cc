#include "./path_length.hh"
#include "../../routeerror.hh"
#include <format>


namespace kiwi::algo {

    auto track_pos_to_cobs(const hardware::Track* track) -> std::Vector<hardware::COBCoord>;
    auto shared_cobs(
        const std::Vector<hardware::COBCoord>& cobs1, const std::Vector<hardware::COBCoord>& cobs2
    ) -> std::Vector<hardware::COBCoord>;

    namespace {

        // Split the path into overlapping maximal runs: consecutive tracks whose COB sets have a
        // non-empty global intersection (same COB "star"). The last track of a run is the first
        // track of the next run so pivot tracks (e.g. exit COB0 / enter COB1) are not dropped.
        template <typename GetTrack, typename PathToString>
        auto cob_compressed_path_length(
            const std::usize n, GetTrack&& get_track, PathToString&& path_to_string_for_error
        ) -> std::usize {
            if (n <= 2) {
                return n;
            }
            std::usize total = 0;
            bool first_chunk = true;
            std::usize head = 0;
            while (head < n) {
                auto current = track_pos_to_cobs(get_track(head));
                std::usize tail = head;
                while (tail + 1 < n) {
                    auto nxt = shared_cobs(current, track_pos_to_cobs(get_track(tail + 1)));
                    if (nxt.empty()) {
                        break;
                    }
                    current = std::move(nxt);
                    ++tail;
                }
                const std::usize len = tail - head + 1;
                if (len == 1 && tail + 1 < n) {
                    std::String message = std::format(
                        "the path is discontinuous from index = {} to index = {}. {}",
                        head,
                        tail + 1,
                        path_to_string_for_error()
                    );
                    throw FinalError(message);
                }
                const std::usize chunk = (len >= 3) ? 2U : len;
                if (first_chunk) {
                    total += chunk;
                    first_chunk = false;
                } else {
                    total += chunk - 1;
                }
                if (tail == n - 1) {
                    break;
                }
                head = tail;
            }
            return total;
        }

    } // namespace

    auto path_length(const routed_path& path, bool switch_length) -> std::usize {
        if (path.size() == 0) {
            debug::info("path_length: path is empty");
            return 0;
        }

        if (switch_length) {
            return path.size();
        }
        return cob_compressed_path_length(
            path.size(),
            [&](std::usize i) { return std::get<0>(path.at(i)); },
            [&]() { return path_to_string(path); }
        );
    }

    auto path_length(const std::Vector<hardware::Track*>& path, bool switch_length) -> std::usize {
        if (path.size() == 0) {
            debug::info("path_length: path is empty");
            return 0;
        }

        if (switch_length) {
            return path.size();
        }
        return cob_compressed_path_length(
            path.size(),
            [&](std::usize i) { return path.at(i); },
            [&]() { return path_to_string(path); }
        );
    }

    auto path_to_string(const std::Vector<hardware::Track*>& path) -> std::String {
        std::String path_str {"Path:\n"};
        for (auto& t : path) {
            path_str += std::format("{}\n", t->coord());
        }
        return path_str;
    }

    auto path_to_string(const routed_path& path) -> std::String {
        std::String path_str {"Path:\n"};
        for (auto& [t, _] : path) {
            path_str += std::format("{}\n", t->coord());
        }
        return path_str;
    }

    // return all cobs connected with track
    auto track_pos_to_cobs(const hardware::Track* track) -> std::Vector<hardware::COBCoord> {
        std::Vector<hardware::COBCoord> cobs {};
        auto coord {track->coord()};
        cobs.emplace_back(coord.row, coord.col);
        if (coord.dir == hardware::TrackDirection::Horizontal) {
            if (coord.col >= 1) {
                cobs.emplace_back(coord.row, coord.col - 1);
            }
        } else if (coord.dir == hardware::TrackDirection::Vertical) {
            if (coord.row >= 1) {
                cobs.emplace_back(coord.row - 1, coord.col);
            }
        }
        return cobs;
    }

    auto shared_cobs(
        const std::Vector<hardware::COBCoord>& cobs1, const std::Vector<hardware::COBCoord>& cobs2
    ) -> std::Vector<hardware::COBCoord> {
        std::Vector<hardware::COBCoord> shared_cobs {};
        for (auto& c1 : cobs1) {
            for (auto& c2 : cobs2) {
                if (c1 == c2) {
                    shared_cobs.emplace_back(c1);
                }
            }
        }
        return shared_cobs;
    }

} // namespace kiwi::algo
