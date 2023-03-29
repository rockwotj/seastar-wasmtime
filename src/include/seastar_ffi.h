#include <memory>
#include <seastar/core/future-util.hh>
#include <seastar/core/reactor.hh>

#pragma once

namespace wasm {

struct SeastarVoidFuture {
    seastar::future<> inner;
};

bool poll_seastar_future(const std::unique_ptr<SeastarVoidFuture>& future);

std::unique_ptr<SeastarVoidFuture> seastar_sleep() noexcept;

}