#include "seastar_ffi.h"
#include <seastar/core/sleep.hh>

namespace wasm {
    namespace {
static seastar::logger lg("ffi-log");
    }

bool poll_seastar_future(const std::unique_ptr<SeastarVoidFuture>& future) {
    if (future->inner.available()) {
        return true;
    }
    if (future->inner.failed()) {
        future->inner.get();
    }
    return false;
}

std::unique_ptr<SeastarVoidFuture> seastar_sleep() noexcept {
    lg.info("Hello from inside WASM FFI function!");
    return std::make_unique<SeastarVoidFuture>(seastar::sleep(std::chrono::seconds(1)));
}
}
