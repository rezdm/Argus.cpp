#include "test_result.h"

#include "types.h"

test_result::test_result(const bool success, const long duration_ms,
                         const std::chrono::system_clock::time_point timestamp,
                         std::optional<std::string> error)
    : success(success), duration_ms(duration_ms), timestamp(timestamp), error(std::move(error)) {
}

test_result_impl::test_result_impl(const bool success, const long duration_ms,
                                  const std::chrono::system_clock::time_point timestamp,
                                  std::optional<std::string> error)
    : success_(success), duration_ms_(duration_ms), timestamp_(timestamp), error_(std::move(error)) {
}