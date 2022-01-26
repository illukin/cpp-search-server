#pragma once

#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Y) X##Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)
#define LOG_DURATION_STREAM(x, y) LogDuration UNIQUE_VAR_NAME_PROFILE(x, y)

class LogDuration {
public:
  // заменим имя типа std::chrono::steady_clock
  // с помощью using для удобства
  using Clock = std::chrono::steady_clock;

  LogDuration(const std::string &id, std::ostream &stream)
    : id_(id), stream_(stream) {}

  ~LogDuration() {
    using namespace std::chrono;
    using namespace std::literals;

    const auto end_time = Clock::now();
    const auto dur = end_time - start_time_;
    stream_ << id_ << ": "s << duration_cast<microseconds>(dur).count()
      << " µs"s << std::endl;
  }

private:
  const std::string id_;
  const Clock::time_point start_time_ = Clock::now();
  std::ostream &stream_;
};
