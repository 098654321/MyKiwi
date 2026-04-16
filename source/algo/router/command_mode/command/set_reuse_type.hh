#pragma once

#include <algo/router/command_mode/command.hh>


namespace PR_tool::algo {

class Set_reuse_type: public Command {
    
public:
    Set_reuse_type() = default;
    ~Set_reuse_type() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;
};

}