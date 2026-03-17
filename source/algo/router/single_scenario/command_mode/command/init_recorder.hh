#pragma once

#include <algo/router/single_scenario/command_mode/command.hh>


namespace kiwi::algo {

class Init_recorder: public Command {
public:
    Init_recorder(bool path_exists_in_other_modes);
    ~Init_recorder() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;

private:
    bool _path_exists_in_other_modes;
};

}