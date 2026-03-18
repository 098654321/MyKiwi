#pragma once

#include "std/file.hh"

namespace kiwi::hardware {
    class Interposer;
}

namespace kiwi::circuit {
    class BaseDie;
}

namespace kiwi::parse {

    auto output_from_routing_results(hardware::Interposer* interposer, const std::FilePath& output_path, circuit::BaseDie* basedie, int mode) -> void;
    auto output_two_modes_from_routing_results(hardware::Interposer* interposer, const std::FilePath& output_path, circuit::BaseDie* basedie, int mode1 = 1, int mode2 = 2) -> void;
    auto write_control_bits(hardware::Interposer* interposer, const std::FilePath& output_path, int mode) -> void;
    auto connect_registers(hardware::Interposer* interposer, circuit::BaseDie* basedie, int mode) -> void;

}