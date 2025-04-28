#include "./path_length.hh"
#include "../../routeerror.hh"
#include <format>


namespace kiwi::algo {

    auto path_length(const routed_path& path, bool switch_length) -> std::usize {
        if (path.size() == 0){
            throw FinalError("path_length: path is empty");
        }

        // path length = number of tracks
        if (switch_length){
            return path.size();
        }
        // for a group with number of consecutive tracks connected with the same COB >= 3, the length of the group is 2
        // because those tracks in the middle are not truely used as signal pathways
        else{
            std::usize head{0}, tail{0};
            std::usize path_length{0};

            if (path.size() <= 2){
                return path.size();
            }
            while(tail != path.size() - 1){
                auto current_pos {track_pos_to_cobs(std::get<0>(path.at(head)))};
                while (true){
                    auto tail_cobs {track_pos_to_cobs(std::get<0>(path.at(tail)))};
                    current_pos = shared_cobs(current_pos, tail_cobs);
                    if (current_pos.empty()){
                        if (tail - head >= 3){
                            path_length += 2;
                            head = tail;
                            break;
                        }
                        else if (tail - head == 2){
                            path_length += 1;
                            tail = head = head + 1;
                            break;
                        }
                        else {
                            std::String message = std::format(
                                "the path is discontinuous from index = {} to index = {}. {}",
                                head,
                                tail,
                                path_to_string(path)
                            );
                            throw FinalError(message);
                        }
                    }
                    else{
                        if (tail == path.size() - 1){
                            break;
                        }
                        tail += 1;
                    }
                }
            }
            if (tail == head){
                path_length += 1;
            }
            else{
                path_length += 2;
            }

            return path_length;
        }
    }

    auto path_length(const std::Vector<hardware::Track*>& path, bool switch_length) -> std::usize {
        if (path.size() == 0) {
            throw FinalError("path_length: path is empty");
        }

        // path length = number of tracks
        if (switch_length){
            return path.size();
        }
        // for a group with number of consecutive tracks connected with the same COB >= 3, the length of the group is 2
        // because those tracks in the middle are not truely used as signal pathways
        else{
            std::usize head{0}, tail{0};
            std::usize path_length{0};

            if (path.size() <= 2){
                return path.size();
            }
            while(tail != path.size() - 1){
                auto current_pos {track_pos_to_cobs(path.at(head))};
                while (true){
                    auto tail_cobs {track_pos_to_cobs(path.at(tail))};
                    current_pos = shared_cobs(current_pos, tail_cobs);
                    if (current_pos.empty()){
                        if (tail - head >= 3){
                            path_length += 2;
                            head = tail;
                            break;
                        }
                        else if (tail - head == 2){
                            path_length += 1;
                            tail = head = head + 1;
                            break;
                        }
                        else {
                            std::String message = std::format(
                                "the path is discontinuous from index = {} to index = {}. {}",
                                head,
                                tail,
                                path_to_string(path)
                            );
                            throw FinalError(message);
                        }
                    }
                    else{
                        if(tail == path.size() - 1){
                            break;
                        }
                        tail += 1;
                    }
                }
            }
            if (tail == head){
                path_length += 1;
            }
            else{
                path_length += 2;
            }

            return path_length;
        }
    }

    auto path_to_string(const std::Vector<hardware::Track*>& path) -> std::String{
        std::String path_str {"Path:\n"};
        for (auto& t: path){
            path_str += std::format("{}\n", t->coord());
        }
        return path_str;
    }

    auto path_to_string(const routed_path& path) -> std::String {
        std::String path_str {"Path:\n"};
        for (auto& [t, _]: path){
            path_str += std::format("{}\n", t->coord());
        }
        return path_str;
    }

    // return all cobs connected with track
    auto track_pos_to_cobs(const hardware::Track* track) -> std::Vector<hardware::COBCoord>{
        std::Vector<hardware::COBCoord> cobs {};
        auto coord {track->coord()};
        cobs.emplace_back(coord.row, coord.col);
        if (coord.dir == hardware::TrackDirection::Horizontal){
            if (coord.col >= 1){
                cobs.emplace_back(coord.row, coord.col - 1);
            }
        }
        else if (coord.dir == hardware::TrackDirection::Vertical){
            if (coord.row >= 1){
                cobs.emplace_back(coord.row - 1, coord.col);
            }
        }
        return cobs;
    }
    
    auto shared_cobs(
        const std::Vector<hardware::COBCoord>& cobs1, const std::Vector<hardware::COBCoord>& cobs2
    ) -> std::Vector<hardware::COBCoord>{
        std::Vector<hardware::COBCoord> shared_cobs {};
        for (auto& c1: cobs1){
            for (auto& c2: cobs2){
                if (c1 == c2){
                    shared_cobs.emplace_back(c1);
                }
            }
        }
        return shared_cobs;
    }


}

