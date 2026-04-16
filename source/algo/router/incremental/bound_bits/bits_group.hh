#pragma once

#include <std/collection.hh>
#include <std/integer.hh>
#include <std/utility.hh>
#include <stdexcept>
#include <algorithm>
#include <debug/debug.hh>
#include <format>


namespace PR_tool::algo {

template <std::usize N>
struct BitsGroup {
    static const std::usize GroupSize = N;

    BitsGroup() : _group_record{} {}
    ~BitsGroup() = default;

    auto set_group_record(std::usize reuse_number, std::usize nonre_number) -> void {
        this->set_reuse_number(reuse_number);
        this->set_nonreuse_number(nonre_number);
    }

    auto set_reuse_number(std::usize reuse_number) -> void {
        auto& [re_num, nonre_num] = this->_group_record;
        re_num = reuse_number;
    }

    auto set_nonreuse_number(std::usize nonre_nubmer) -> void {
        auto& [re_num, nonre_num] = this->_group_record;
        nonre_num = nonre_nubmer;
    }

    auto add_reuse_number(std::usize number = 1) -> void {
        auto& [re_num, nonre_num] = this->_group_record;
        re_num += number;
    }

    auto add_nonreuse_number(std::usize number = 1) -> void {
        auto& [re_num, nonre_num] = this->_group_record;
        nonre_num += number;
    }

    auto reuse_number() const -> std::usize {return std::get<0>(this->_group_record);}
    auto nonreuse_number() const -> std::usize {return std::get<1>(this->_group_record);}

    auto to_string() const -> std::String {
        return std::format(
            "reuse = {}, non_reuse = {}, unused = {}\n", this->reuse_number(), this->nonreuse_number(), this->GroupSize - this->reuse_number() - this->nonreuse_number()
        );
    }

    std::Tuple<std::usize, std::usize> _group_record;
};

}
