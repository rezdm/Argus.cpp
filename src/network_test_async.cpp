#include "network_test_async.h"

test_result network_test_async::execute(const test_config& config, const int timeout_ms) const {
    // Synchronous wrapper - execute async and wait for result
    auto future = execute_async(config, timeout_ms);
    return future.get();
}