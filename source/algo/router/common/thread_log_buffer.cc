#include "./thread_log_buffer.hh"

namespace kiwi::algo {

    thread_local std::String* g_thread_log_buffer = nullptr;

    ScopedThreadLogBuffer::ScopedThreadLogBuffer(std::String& buffer): _prev(g_thread_log_buffer) {
        g_thread_log_buffer = &buffer;
    }

    ScopedThreadLogBuffer::~ScopedThreadLogBuffer() {
        g_thread_log_buffer = _prev;
    }

    auto append_thread_log_line(const std::String& line) -> void {
        if (g_thread_log_buffer != nullptr) {
            *g_thread_log_buffer += line;
            *g_thread_log_buffer += '\n';
        } else {
            debug::info(line);
        }
    }

}
