#include <parse/comparator/controlbits_parser.hh>


using namespace kiwi;


static void test_comparint_two_mode_controlbits() {
    for (int c = 1; c <= 20 ; c++) {
        std::string file2 = "..\\algorithm\\incremental_routing\\2026.01.21_test\\test2_cycles\\cycle" + std::to_string(c) + ".txt";
        std::string file1 = "..\\algorithm\\incremental_routing\\2026.01.21_test\\test2\\mode1_5.txt";
        parse::compare(file1, file2);
    }
}

void test_comparator_main() {
    test_comparint_two_mode_controlbits();
}





