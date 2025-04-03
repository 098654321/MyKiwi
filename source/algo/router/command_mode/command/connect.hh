#pragma once

#include <algo/router/command_mode/command.hh>

namespace kiwi::algo {

class Connect: public Command {
public:
    Connect() = default;
    ~Connect() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;

};

}