#include "vulkan_backend/backend.h"

#include <doctest/doctest.h>

#include <stdexcept>
#include <vector>

// No GPU is needed (or assumed) here: these tests cover the failure paths that must be clean
// errors rather than crashes, e.g. on a machine without a Vulkan driver.

namespace {
// A "loader" that can't resolve anything.
extern "C" void* VKAPI_CALL nullProcAddr(VkInstance, const char*) {
    return nullptr;
}
} // namespace

TEST_CASE("Backend rejects a missing loader function") {
    CHECK_THROWS_AS(vkbackend::Backend(nullptr, {}), std::runtime_error);
}

TEST_CASE("Backend rejects a loader that resolves no entry points") {
    void* fn = reinterpret_cast<void*>(&nullProcAddr);
    CHECK_THROWS_AS(vkbackend::Backend(fn, std::vector<const char*>{}), std::runtime_error);
}
