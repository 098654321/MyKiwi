#pragma once

#include "./tobregister.hh"
#include <std/collection.hh>
#include <std/integer.hh>
#include <std/utility.hh>

namespace kiwi::hardware {

    class TOBMuxConnector {
    public:
        TOBMuxConnector(std::usize input_index, std::usize output_index, TOBMuxRegister* reg);

        auto connect() -> void;
        auto give_out() -> void;
        auto stay_inside() -> void;
        auto disconnect() -> void;

        auto input_index() const -> std::usize { return this->_input_index; }
        auto output_index() const -> std::usize { return this->_output_index; }

        auto check_consistency() const -> void;
        auto check_reg_address() const -> uintptr_t;
        auto check_pregister() const -> const TOBMuxRegister* { return this->_register; }

    private:
        std::usize _input_index;
        std::usize _output_index;
        TOBMuxRegister* _register;
    };

    class TOBMux {
    public:
        TOBMux(std::usize mux_size);

    public:
        auto available_connectors(std::usize input_index, bool shared = false) -> std::Vector<TOBMuxConnector>;
        auto available_output_indexes() const -> std::Vector<std::usize>;
        auto connector(std::usize input_index, std::usize output_index, bool give_out = true) -> TOBMuxConnector;

    public:
        auto randomly_map_remain_indexes() -> void;
        auto reset_regs() -> void;
    
    public:
        auto index_map(std::usize input_index) const -> std::Option<std::usize>;
        auto registerr(std::usize input_index) -> TOBMuxRegister*;
        auto registerr(std::usize input_index) const -> const TOBMuxRegister*;
        auto registers() const -> const std::Vector<TOBMuxRegister>& {return this->_registers;}

    public:
        auto mux_size() const -> std::usize { return this->_mux_size; }
    
    private:
        std::usize _mux_size;
        std::Vector<TOBMuxRegister> _registers;     // index = bump_index_in_group
    };

}