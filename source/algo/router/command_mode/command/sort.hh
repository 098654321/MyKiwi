#pragma once

#include <algo/router/command_mode/command.hh>


namespace PR_tool::algo {

class Sort: public Command {
public:
    Sort() = default;
    ~Sort() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;
    
};

}