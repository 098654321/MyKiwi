#include "./module.hh"
#include "./writer.hh"
#include "debug/debug.hh"

namespace kiwi::parse {

    auto write_control_bits(hardware::Interposer* interposer, const std::FilePath& output_path) -> void {
        debug::info_fmt(
            "\n\
            **********************************************************************************\n\
                            Write control bits into '{}'\n\
            **********************************************************************************\
            ", output_path.string()
        );
        auto writer = parse::Writer{interposer};
        writer.fetch_and_write(output_path);

        debug::info_fmt("END\n\n");
    }

}