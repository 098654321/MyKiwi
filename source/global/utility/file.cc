#include "./file.hh"
#include <filesystem>
#include <format>
#include <std/file.hh>
#include <std/string.hh>

namespace PR_tool::utility {

    std::size_t filesize(std::ifstream& file) {
        file.seekg(0, std::ios::end);
        std::size_t size{ static_cast<std::size_t>(file.tellg()) };
        file.seekg(0, std::ios::beg);
        return size;
    }

    std::string read_file(std::InFile& file) {
        std::size_t size {filesize(file)};
        std::string content {};
        content.resize(size + 1);
        file.read(content.data(), size);
        content.resize(size);
        return content;
    }

    std::string read_file(const std::filesystem::path& filepath) {
        auto infile = std::InFile{filepath};
        if (!infile.is_open()) 
            throw std::runtime_error {std::format("util::read_file: File '{}' does not open.", filepath.string())};
        auto content = read_file(infile);
        infile.close();
        return content;
    }

    void append_logs(std::String input_file, std::String output_file) {
        std::ifstream src(input_file, std::ios::binary);
        std::ofstream dst(output_file, std::ios::binary | std::ios::app);

        if (!src.is_open()) {
            throw std::runtime_error {std::format("util::append_logs: File '{}' does not open.", input_file)};
        }

        if (!dst.is_open()) {
            throw std::runtime_error {std::format("util::append_logs: File '{}' does not open.", output_file)};
        }

        dst << src.rdbuf();

        src.close();
        dst.close();
    }

}