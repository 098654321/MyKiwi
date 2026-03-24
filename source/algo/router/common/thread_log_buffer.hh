#pragma once

#include <debug/debug.hh>
#include <std/collection.hh>
#include <format>

namespace kiwi::algo {

    extern thread_local std::String* g_thread_log_buffer;

    struct ScopedThreadLogBuffer {
        explicit ScopedThreadLogBuffer(std::String& buffer);
        ~ScopedThreadLogBuffer();
        std::String* _prev;
    };

    auto append_thread_log_line(const std::String& line) -> void;

    template <typename... Args>
    auto log_info_fmt(std::format_string<Args...> fmt, Args&&... args) -> void {
        auto line = std::format(fmt, std::forward<Args>(args)...);
        if (g_thread_log_buffer != nullptr) {
            append_thread_log_line(line);
        } else {
            debug::info(line);
        }
    }

    template <typename... Args>
    auto log_warning_fmt(std::format_string<Args...> fmt, Args&&... args) -> void {
        auto line = std::format(fmt, std::forward<Args>(args)...);
        if (g_thread_log_buffer != nullptr) {
            append_thread_log_line(line);
        } else {
            debug::warning(line);
        }
    }

    template <typename... Args>
    auto log_error_fmt(std::format_string<Args...> fmt, Args&&... args) -> void {
        auto line = std::format(fmt, std::forward<Args>(args)...);
        if (g_thread_log_buffer != nullptr) {
            append_thread_log_line(line);
        } else {
            debug::error(line);
        }
    }

}
