#include <parse/comparator/controlbits_parser.hh>


using namespace kiwi;


static void test_comparint_two_mode_controlbits() {
    parse::compare("controlbits_1.txt", "controlbits_2.txt");
}

void test_comparator_main() {
    test_comparint_two_mode_controlbits();
}





