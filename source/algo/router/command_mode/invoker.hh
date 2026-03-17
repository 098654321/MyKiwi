#pragma once

#include "command/commands.hh"
#include <global/std/memory.hh>
#include <global/std/collection.hh>
#include "../routeerror.hh"
#include "../routeengine.hh"


namespace kiwi::circuit {
    
    class BaseDie;

}


namespace kiwi::hardware {

    class Interposer;

}


namespace kiwi::algo {

struct RouteStrategy;

enum class CommandType {
    Clear,
    Reroute,
    Resources,
    Allocate,
    Route,
    Sort,
    Incre_route,
    Init_recorder,
    Set_reuse_type
};

class Invoker {
public:
    Invoker(): _commands{}, _current_command{nullptr} {}
    ~Invoker() noexcept = default;

public:
    auto invoke(hardware::Interposer*, RouteEngine&) -> void;
    auto set_route_commands(bool incremental, bool path_exists) -> void;
    auto call_remediation(Command*) -> bool;
    auto check_command() -> bool;

public:
    static auto create_command(CommandType type, bool path_exists) -> std::Rc<Command>;

public:
    auto current_command() const -> Command* {
        return this->_current_command.get();
    }

private:
    std::Deque<std::Rc<Command>> _commands;    // execute the command at the head and pop it on each loop
    std::Rc<Command> _current_command;

};

}
