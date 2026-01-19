#pragma once

#include <algo/router/command_mode/command.hh>


namespace kiwi::algo {

class Init_recorder: public Command {
public:
    Init_recorder(bool try_all_modes, bool path_exists_in_other_modes);
    ~Init_recorder() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;

private:
    bool _try_all_modes;
    bool _path_exists_in_other_modes;
};

}