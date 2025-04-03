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
    Connect,
    Reroute,
    Resources,
    Allocate,
    Route,
    Sort,
    CheckResult
};

class Invoker {
public:
    Invoker(): _commands{}, _current_command{nullptr} {}
    ~Invoker() noexcept = default;

public:
    auto invoke(hardware::Interposer*, RouteEngine&) -> void;
    auto set_route_commands() -> void;
    auto call_remediation(Command*) -> bool;
    auto check_command() -> bool;

public:
    static auto create_command(CommandType type) -> std::Rc<Command>;

public:
    auto current_command() const -> Command* {
        return this->_current_command.get();
    }

private:
    std::Deque<std::Rc<Command>> _commands;    // execute the command at the head and pop it on each loop
    std::Rc<Command> _current_command;

};

}
