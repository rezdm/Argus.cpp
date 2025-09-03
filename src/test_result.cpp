#include "test_result.h"

test_result::test_result(bool success, long duration_ms, 
                        std::chrono::system_clock::time_point timestamp,
                        std::optional<std::string> error)
    : success(success), duration_ms(duration_ms), timestamp(timestamp), error(std::move(error)) {
}

test_result_impl::test_result_impl(bool success, long duration_ms, 
                                  std::chrono::system_clock::time_point timestamp,
                                  std::optional<std::string> error)
    : success_(success), duration_ms_(duration_ms), timestamp_(timestamp), error_(std::move(error)) {
}