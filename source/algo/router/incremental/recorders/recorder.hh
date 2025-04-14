#pragma once

#include <global/std/collection.hh>
#include <global/std/utility.hh>
#include <global/std/integer.hh>
#include <stdexcept>


namespace kiwi::algo {

const std::usize BASICCOST = 5;
const float EPSILON = 1;
const float GROUPCOEF = 0.9;
const float HISTORYCOEF = 0.1;
const std::usize TOBMUXGROUPSIZE = 8;
const std::usize TRACKGROUPSIZE = 32;

class TypeRecorder {
public:
    TypeRecorder() : _reuse_type(std::nullopt), _history({0, 0}){}
    TypeRecorder(bool reuse_type) : _reuse_type(reuse_type), _history({0, 0}){}
    ~TypeRecorder() = default;

public:
    auto current_type() const -> std::Option<bool> {return this->_reuse_type;}
    auto re_history_times() const -> std::usize {return this->_history.first;}
    auto non_re_history_times() const -> std::usize {return this->_history.second;}

public:
    auto set_type(bool reuse_type) -> void {
        this->_reuse_type = reuse_type;
    }

    auto set_re_history(std::usize times) -> void {
        this->_history.first = times;
    }

    auto set_non_re_history(std::usize times) -> void {
        this->_history.second = times;
    }

    auto update_history(bool reuse_type) -> void {
        reuse_type ? this->_history.first++ : this->_history.second++;
    }

    auto update(bool reuse_type) -> void {
        this->set_type(reuse_type);
        this->update_history(reuse_type);
    }

    auto cost(std::usize reuse_num, std::usize nonre_num) const -> float {
        if (!this->_reuse_type.has_value()) {
            return BASICCOST;
        }
        auto group_ratio = this->_reuse_type ? (float)(reuse_num-nonre_num)/(float)(reuse_num + EPSILON) : (float)(nonre_num-reuse_num)/(float)(nonre_num + EPSILON);
        auto history_ratio = (this->_reuse_type ? (float)(reuse_num-nonre_num) : (float)(nonre_num-reuse_num)) / (nonre_num+reuse_num+EPSILON);
        return BASICCOST * (1-GROUPCOEF*group_ratio-HISTORYCOEF*history_ratio);
    }

private:
    std::Option<bool> _reuse_type;                           // true if reusable
    std::Pair<std::usize, std::usize> _history; // <reusable_times, non_reusable_times>
};

class SharedRecorder {
public:
    SharedRecorder(): _shared{0}, _history_shared{0} {}
    ~SharedRecorder() = default;

public:
    auto shared_times() const -> std::usize {return this->_shared;}
    auto history_shared_times() const -> std::usize {return this->_history_shared;}
    auto set_shared_times(std::usize times) -> void {this->_shared = times;}
    auto set_history_shared_times(std::usize times) -> void {this->_history_shared = times;}

    auto cost() const -> float {
        return (BASICCOST + this->_shared) * this->_history_shared;
    }

    auto update() -> void {
        this->_shared++;
        this->_history_shared++;
    }
    
    auto remove_shared() -> void {
        if (this->_shared > 0){
            this->_shared--;
        }
    }

private:
    std::usize _shared;
    std::usize _history_shared;
};

}
