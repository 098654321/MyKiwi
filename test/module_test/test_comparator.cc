#include <parse/comparator/controlbits_parser.hh>


using namespace kiwi;


static void test_comparint_two_mode_controlbits() {
    // std::string file1{"..\\algorithm\\incremental_routing\\2025_10_30_test\\test_mode1_with_standard_file_of_mode2\\controlbits_comparison_in_cycles\\15\\4_controlbits_1.txt"};
    std::string file1{"controlbits_1.txt"};
    // std::string file2{"..\\algorithm\\incremental_routing\\2025_10_30_test\\test_mode1_with_standard_file_of_mode2\\controlbits_standard.txt"};
    std::string file2{"controlbits_2.txt"};
    parse::compare(file1, file2);
}

void test_comparator_main() {
    test_comparint_two_mode_controlbits();
}





