# Seastar + Wasmtime + CXX

This repo is an adventure of experimenting with seastar, wasmtime, and cxx

The goal is to be able to call wasmtime from seastar such that wasm functions yield control after some time back to the seastar scheduler.

## Building

Setup: `./seastar/install-dependencies.sh`
Configure: `cmake --preset default`
Build: `cmake --build --preset default`