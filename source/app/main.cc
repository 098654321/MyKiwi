#include <debug/debug.hh>
#include <std/exception.hh>

namespace PR_tool {
    int main(int argc, char** argv);
}

int main(int argc, char** argv) try {
    return PR_tool::main(argc, argv);
} catch (const std::Exception& err) {
    PR_tool::debug::error_fmt("PR_tool failed: {}", err.what());
    std::exit(1);
}