#pragma once

#include <algo/router/command_mode/command.hh>

namespace kiwi::algo {

class Resources: public Command {
public:
    Resources() = default;
    ~Resources() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;
    
};

}

