#include <vector>
#include <iostream>

#include <seastar/core/app-template.hh>
#include <seastar/core/coroutine.hh>
#include <seastar/core/future-util.hh>
#include <seastar/core/reactor.hh>
#include <seastar/coroutine/maybe_yield.hh>
#include <seastar/core/sleep.hh>
#include <seastar/util/log.hh>

#include "cxxbridge-rust/lib.h"

static seastar::logger lg("main-log");

namespace {
    seastar::future<> RunWasmSleeperLoop(const bool& stopped) {
        // There is a fair amount of concepts just to get setup in wasmtime, here's the docs that maybe helpful:
        // https://docs.wasmtime.dev/api/wasmtime/index.html#core-concepts
        auto engine = wasm::create_engine();
        auto loop_module = engine->compile_module(R"WAT(
            (module
                ;; This is defined in seastar_ffi and the Rust code links it into this module.
                (import "host" "sleep" (func $sleep))
                (func $loop_forever
                    (loop $my_loop
                        call $sleep
                        br $my_loop)
                )
                (export "loop_forever" (func $loop_forever))
            )
        )WAT");
        auto store = engine->create_store();
        auto instance = store->create_instance(*engine, *loop_module);
        auto func_handle = instance->lookup_function(*store, "loop_forever");
        auto running_func = func_handle->invoke(*store);
        bool done = false;
        // A busy loop where we poll the WASM VM to see if it's completed it's work
        // Make sure to yield so the seastar scheduler can do other work.
        while (!done && !stopped) {
            done = running_func->pump();
            co_await seastar::yield();
        }
    }

    seastar::future<> RunWasmBusyLoop(const bool& stopped) {
        auto engine = wasm::create_engine();
        // Create a wasm spin loop to show that the VM will yield control after some time.
        // If you want to see how that works see the fuel related code in Rust, essentially after some 
        // amount of computation, the runtime pauses the existing fiber and yields control back to the caller.
        // Docs: https://docs.wasmtime.dev/api/wasmtime/struct.Store.html#method.out_of_fuel_async_yield
        auto loop_module = engine->compile_module(R"WAT(
            (module
                (func $loop_forever
                    (loop $my_loop
                        br $my_loop)
                )
                (export "loop_forever" (func $loop_forever))
            )
        )WAT");
        auto store = engine->create_store();
        auto instance = store->create_instance(*engine, *loop_module);
        auto func_handle = instance->lookup_function(*store, "loop_forever");
        auto running_func = func_handle->invoke(*store);
        bool done = false;
        while (!done && !stopped) {
            lg.info("Hello from WASM busy loop!");
            done = running_func->pump();
            co_await seastar::yield();
        }
    }
}

int main(int argc, char **argv) {
    seastar::app_template app;
    bool stopped = false;
    return app.run(argc, argv, [&] {
        seastar::engine().at_exit([&] {
            stopped = true;
            return seastar::make_ready_future();
        });
        auto wasm_sleeper_task = RunWasmSleeperLoop(stopped);
        auto wasm_busy_task = RunWasmBusyLoop(stopped);
        // Loop forever printing stuff to show the above code isn't blocking
        auto logger_task = seastar::do_until([&stopped] { return stopped; }, [] {
            lg.info("Hello from seastar loop!");
            return seastar::sleep(std::chrono::seconds(1));
        });
        return seastar::when_all_succeed(std::move(wasm_sleeper_task), std::move(wasm_busy_task), std::move(logger_task))
            .then([](auto) { return seastar::make_ready_future<int>(0); });
    });
}