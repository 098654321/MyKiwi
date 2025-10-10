#include <global/std/collection.hh>
#include <global/std/string.hh>
#include <global/std/file.hh>


namespace kiwi::parse {

// store the control bits in the form of unordered_map<reg_name, value>
struct ControlBits {
public:
    ControlBits(const std::string& file);
    ~ControlBits() noexcept = default;

public:
    std::string _file_name;
    std::unordered_map<std::String, std::String> _bits;
    std::size_t _lines;
};


// compare the control bits between two files
void compare_bits(const ControlBits& bits1, const ControlBits& bits2);

void compare(std::string file1, std::string file2);

}

