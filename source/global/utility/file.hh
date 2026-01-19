#pragma once

#include <std/file.hh>
#include <std/string.hh>

namespace kiwi::utility {

    auto read_file(const std::FilePath& filepath) -> std::String;

    auto append_logs(std::String input_file, std::String output_file) -> void;

}