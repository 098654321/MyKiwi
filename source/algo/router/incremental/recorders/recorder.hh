#pragma once

#include <global/std/collection.hh>
#include <global/std/utility.hh>
#include <global/std/integer.hh>
#include <stdexcept>
#include <algorithm>
#include <format>
#include <string>
#include <debug/debug.hh>


namespace kiwi::algo {

const float BASICCOST = 5;
const float EPSILON = 5;
const float GROUPCOEF = 0.5;
const float HISTORYCOEF = 0.5;
const float TOBMUXGROUPSIZE = 8;
const float TRACKGROUPSIZE = 32;

class TypeRecorder {    // 单个单位的 recorder
public:
    TypeRecorder(bool use_cost) : _reuse_type(std::nullopt), _history({0, 0}), _cost_nonreuse{BASICCOST}, _cost_reuse{BASICCOST}, _use_cost{use_cost} {}
    TypeRecorder(bool reuse_type, bool use_cost) : _reuse_type(reuse_type), _history({0, 0}), _cost_nonreuse{BASICCOST}, _cost_reuse{BASICCOST}, _use_cost{use_cost} {}
    ~TypeRecorder() = default;

public:
    auto current_type() const -> std::Option<bool> {return this->_reuse_type;}
    auto re_history_times() const -> float {return this->_history.first;}
    auto non_re_history_times() const -> float {return this->_history.second;}

public:
    auto set_use_cost(bool use_cost) -> void {this->_use_cost = use_cost;}

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

    auto update_cost(float reuse_num, float nonre_num) -> float {
        if (!this->_reuse_type.has_value()) {
            return BASICCOST;
        }

        auto h_reuse_n = std::get<0>(this->_history);
        auto h_nonre_n = std::get<1>(this->_history);
        auto history_ratio = (h_reuse_n-h_nonre_n) / (h_reuse_n+h_nonre_n+EPSILON);
        auto group_ratio = (reuse_num-nonre_num) / (nonre_num + reuse_num + EPSILON);

        this->_cost_reuse = BASICCOST * (1-GROUPCOEF*group_ratio-HISTORYCOEF*history_ratio);
        this->_cost_nonreuse = BASICCOST * (1+GROUPCOEF*group_ratio+HISTORYCOEF*history_ratio);
    }

    auto cost(bool reuse_type) const -> float {return this->_use_cost ? (reuse_type ? this->_cost_reuse : this->_cost_nonreuse) : BASICCOST;}

    auto reset_type() -> void {
        this->_reuse_type.reset();
    }

    auto re_initialize() -> void {
        this->reset_type();
        this->_history = {0, 0};
        this->_cost_reuse = BASICCOST;
        this->_cost_nonreuse = BASICCOST;
    }

    auto show_data(bool print = false) const -> std::string {
        std::string msg = "";
        if (this->_reuse_type.has_value()) {
            msg += "Current type: " + std::to_string(this->_reuse_type.value()) + ", ";
        }
        else {
            msg += "Current type: None, ";
        }
        msg += "History reusable times: " + std::to_string(this->_history.first) + ", ";
        msg += "History non-reusable times: " + std::to_string(this->_history.second) + ", ";
        msg += "Cost reuse: " + std::to_string(this->_cost_reuse) + ", ";
        msg += "Cost non-reuse: " + std::to_string(this->_cost_nonreuse) + "\n";
        
        if (print) {
            debug::info(msg);
        }
        return msg;

    }

private:
    std::Option<bool> _reuse_type;       // true if reusable
    std::Pair<float, float> _history;   // <reusable_times, non_reusable_times>
    float _cost_reuse;                  // if called by reuse net
    float _cost_nonreuse;               // if called by non-reuse net
    bool _use_cost;
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
