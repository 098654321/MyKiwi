#include <parse/comparator/controlbits_parser.hh>


using namespace kiwi;


static void test_comparint_two_mode_controlbits() {
    std::string file1{"..\\algorithm\\incremental_routing\\2025.11.6_test\\test_2\\controlbits_standard_file.txt"};
    // std::string file1{"controlbits_1.txt"};
    std::string file2{"..\\algorithm\\incremental_routing\\2025.11.6_test\\test_2\\controlbits_load5_2.txt"};
    // std::string file2{"controlbits_2.txt"};
    parse::compare(file1, file2);
}

void test_comparator_main() {
    test_comparint_two_mode_controlbits();
}





