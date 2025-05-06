#pragma once

#include <global/std/collection.hh>
#include <global/std/utility.hh>
#include <global/std/integer.hh>
#include <stdexcept>
#include <algorithm>


namespace kiwi::algo {

const float BASICCOST = 5;
const float EPSILON = 10;
const float GROUPCOEF = 0.5;
const float HISTORYCOEF = 0.5;
const float TOBMUXGROUPSIZE = 8;
const float TRACKGROUPSIZE = 32;

class TypeRecorder {
public:
    TypeRecorder() : _reuse_type(std::nullopt), _history({0, 0}){}
    TypeRecorder(bool reuse_type) : _reuse_type(reuse_type), _history({0, 0}){}
    ~TypeRecorder() = default;

public:
    auto current_type() const -> std::Option<bool> {return this->_reuse_type;}
    auto re_history_times() const -> float {return this->_history.first;}
    auto non_re_history_times() const -> float {return this->_history.second;}

public:
    auto set_type(bool reuse_type) -> void {
        this->_reuse_type = reuse_type;
    }

    auto set_re_history(float times) -> void {
        this->_history.first = times;
    }

    auto set_non_re_history(float times) -> void {
        this->_history.second = times;
    }

    auto update_history(bool reuse_type) -> void {
        reuse_type ? this->_history.first++ : this->_history.second++;
    }

    auto update(bool reuse_type) -> void {
        this->set_type(reuse_type);
        this->update_history(reuse_type);
    }

    auto cost(float reuse_num, float nonre_num) const -> float {
        if (!this->_reuse_type.has_value()) {
            return BASICCOST;
        }

        auto h_reuse_n = std::get<0>(this->_history);
        auto h_nonre_n = std::get<1>(this->_history);
        auto history_ratio = (this->_reuse_type ? (h_reuse_n-h_nonre_n) : (h_nonre_n-h_reuse_n)) / (h_reuse_n+h_nonre_n+EPSILON);
        auto group_ratio = (this->_reuse_type ? (reuse_num-nonre_num) : (nonre_num-reuse_num))/(nonre_num + reuse_num + EPSILON);
        auto final_cost = BASICCOST * (1-GROUPCOEF*group_ratio-HISTORYCOEF*history_ratio);
        return final_cost;
    }

    auto re_initialize() -> void {
        this->_reuse_type = std::nullopt;
        this->_history = {0, 0};
    }

private:
    std::Option<bool> _reuse_type;                           // true if reusable
    std::Pair<float, float> _history; // <reusable_times, non_reusable_times>
};

class SharedRecorder {
public:
    SharedRecorder(): _shared{0}, _history_shared{0} {}
    ~SharedRecorder() = default;

public:
    auto shared_times() const -> float {return this->_shared;}
    auto history_shared_times() const -> float {return this->_history_shared;}
    auto set_shared_times(float times) -> void {this->_shared = times;}
    auto set_history_shared_times(float times) -> void {this->_history_shared = times;}

    auto cost() const -> float {
        return (BASICCOST + this->_shared) * this->_history_shared;
    }

    auto update() -> void {
        this->_shared += 1;
        this->_history_shared += 1;
    }
    
    auto remove_shared() -> void {
        if (this->_shared > 0){
            this->_shared -= 1;
        }
    }

private:
    float _shared;
    float _history_shared;
};

}
