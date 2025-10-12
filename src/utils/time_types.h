#pragma once

#include <chrono>

// Strong type wrappers for time values to prevent mixing up different time units

template <typename Rep, typename Tag>
class strong_time_type {
 public:
  using value_type = Rep;

  constexpr strong_time_type() : value_(Rep{}) {}
  constexpr explicit strong_time_type(Rep value) : value_(value) {}

  [[nodiscard]] constexpr Rep count() const { return value_; }
  [[nodiscard]] constexpr explicit operator Rep() const { return value_; }

  constexpr auto operator<=>(const strong_time_type&) const = default;

  constexpr strong_time_type& operator+=(const strong_time_type& other) {
    value_ += other.value_;
    return *this;
  }

  constexpr strong_time_type& operator-=(const strong_time_type& other) {
    value_ -= other.value_;
    return *this;
  }

  constexpr strong_time_type operator+(const strong_time_type& other) const { return strong_time_type{value_ + other.value_}; }

  constexpr strong_time_type operator-(const strong_time_type& other) const { return strong_time_type{value_ - other.value_}; }

 private:
  Rep value_;
};

// Tag types for different time concepts
struct timeout_tag {};
struct interval_tag {};
struct duration_tag {};

// Concrete types
using timeout_ms = strong_time_type<int, timeout_tag>;
using interval_sec = strong_time_type<int, interval_tag>;
using duration_ms = strong_time_type<long, duration_tag>;

// Conversion functions
[[nodiscard]] constexpr std::chrono::milliseconds to_chrono_ms(const timeout_ms timeout) { return std::chrono::milliseconds(timeout.count()); }

[[nodiscard]] constexpr std::chrono::seconds to_chrono_sec(const interval_sec interval) { return std::chrono::seconds(interval.count()); }

[[nodiscard]] constexpr std::chrono::milliseconds to_chrono_ms(const duration_ms duration) { return std::chrono::milliseconds(duration.count()); }

// Factory functions for readability
[[nodiscard]] constexpr timeout_ms operator""_timeout_ms(const unsigned long long ms) { return timeout_ms{static_cast<int>(ms)}; }

[[nodiscard]] constexpr interval_sec operator""_interval_sec(const unsigned long long sec) { return interval_sec{static_cast<int>(sec)}; }

[[nodiscard]] constexpr duration_ms operator""_duration_ms(const unsigned long long ms) { return duration_ms{static_cast<long>(ms)}; }
