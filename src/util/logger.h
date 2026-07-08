#pragma once

#include <fmt/core.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string_view>

namespace zazaki_git {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Off = 4,
};

inline constexpr const char* log_level_str(LogLevel lv) {
    switch (lv) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        default:              return "????";
    }
}

inline LogLevel g_min_log_level = LogLevel::Info;

inline void set_log_level(LogLevel lv) { g_min_log_level = lv; }

namespace detail {

inline const char* basename(const char* path) {
    const char* last = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') last = p + 1;
    }
    return last;
}

}  // namespace detail

template <typename... Args>
inline void log_message(LogLevel lv,
                         const char* file,
                         int line,
                         const char* func,
                         fmt::format_string<Args...> fmt_str,
                         Args&&... args) {
    if (lv < g_min_log_level) return;

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::tm local_tm{};
    localtime_r(&t, &local_tm);

    char time_buf[32];
    std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d.%03d",
                  local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec,
                  static_cast<int>(ms.count()));

    auto& out = (lv >= LogLevel::Error) ? stderr : stdout;

    fmt::print(out, "[{} {} {}:{} {}] ",
               time_buf,
               log_level_str(lv),
               detail::basename(file),
               line,
               func);

    fmt::print(out, fmt_str, std::forward<Args>(args)...);
    fmt::print(out, "\n");
}

}  // namespace zazaki_git

#define LOG_DEBUG(...) \
    zazaki_git::log_message(zazaki_git::LogLevel::Debug, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...) \
    zazaki_git::log_message(zazaki_git::LogLevel::Info, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...) \
    zazaki_git::log_message(zazaki_git::LogLevel::Warn, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(...) \
    zazaki_git::log_message(zazaki_git::LogLevel::Error, __FILE__, __LINE__, __func__, __VA_ARGS__)
