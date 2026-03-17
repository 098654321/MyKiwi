#include "./module.hh"
#include "./writer.hh"
#include "debug/debug.hh"
#include <circuit/basedie.hh>
#include <hardware/interposer.hh>

namespace kiwi::parse {

    auto output_from_routing_results(hardware::Interposer* interposer, const std::FilePath& output_path, circuit::BaseDie* basedie, int mode) -> void {
        connect_registers(interposer, basedie, mode);
        write_control_bits(interposer, output_path, mode);
        interposer->reset_regs();
    }

    auto write_control_bits(hardware::Interposer* interposer, const std::FilePath& output_path, int mode) -> void {
        std::FilePath control_bits_path = output_path / ("controlbits_" + std::to_string(mode) + ".txt");
        debug::info_fmt(
            "\n\
            **********************************************************************************\n\
                            Write control bits into '{}'\n\
            **********************************************************************************\
            ", control_bits_path.string()
        );
        auto writer = parse::Writer{interposer};
        writer.fetch_and_write(control_bits_path);

        debug::info_fmt("END\n\n");
    }

    auto connect_registers(hardware::Interposer* interposer, circuit::BaseDie* basedie, int mode) -> void {
        debug::info("Connecting paths ...");
        auto nets = basedie->nets_to_vector();
        for (auto& net : nets) {
            if (net->modes().contains(mode)) {
                debug::debug_fmt("{} is connecting paths ...", net->name());

                auto& path_package = net->pathpackage();    
                path_package.connect_all();
            }
            // else {
            //     net->pathpackage().reset_all();
            // }
        }
    }

}