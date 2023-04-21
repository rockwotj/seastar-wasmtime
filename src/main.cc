#include <vector>
#include <iostream>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <wasmedge/wasmedge.h>
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

void run_fib_wasm() {
  /* Create the configure context and add the WASI support. */
  /* This step is not necessary unless you need WASI support. */
  WasmEdge_ConfigureContext *ConfCxt = WasmEdge_ConfigureCreate();
  WasmEdge_ConfigureAddHostRegistration(ConfCxt,
                                        WasmEdge_HostRegistration_Wasi);
  /* The configure and store context to the VM creation can be NULL. */
  WasmEdge_VMContext *VMCxt = WasmEdge_VMCreate(ConfCxt, NULL);

  /* The parameters and returns arrays. */
  WasmEdge_Value Params[1] = {WasmEdge_ValueGenI32(32)};
  WasmEdge_Value Returns[1];
  /* Function name. */
  WasmEdge_String FuncName = WasmEdge_StringCreateByCString("fib");
  /* Run the WASM function from file. */
  WasmEdge_Result Res = WasmEdge_VMRunWasmFromFile(
      VMCxt, "fib.wasm", FuncName, Params, 1, Returns, 1);

  if (WasmEdge_ResultOK(Res)) {
    printf("Get result: %d\n", WasmEdge_ValueGetI32(Returns[0]));
  } else {
    printf("Error message: %s\n", WasmEdge_ResultGetMessage(Res));
  }

  /* Resources deallocations. */
  WasmEdge_VMDelete(VMCxt);
  WasmEdge_ConfigureDelete(ConfCxt);
  WasmEdge_StringDelete(FuncName);
}


auto fib(int32_t n) -> int32_t {
  if (n < 2) {
    return 1;
  }
  return fib(n - 2) + fib(n - 1);
}

void run_fib_native() {
  int32_t Ret = fib(32);
  printf("Get result: %d\n", Ret);
}

void run_fib_wasmtime() {
  auto engine = wasm::create_engine();
  // A WAT version of fib.c
  auto loop_module = engine->compile_module(R"WAT(
  (module
  (type $t0 (func))
  (type $t1 (func (param i32) (result i32)))
  (func $__wasm_call_ctors (export "__wasm_call_ctors") (type $t0))
  (func $fib (export "fib") (type $t1) (param $p0 i32) (result i32)
    (local $l1 i32) (local $l2 i32) (local $l3 i32) (local $l4 i32) (local $l5 i32) (local $l6 i32) (local $l7 i32) (local $l8 i32) (local $l9 i32) (local $l10 i32) (local $l11 i32) (local $l12 i32) (local $l13 i32) (local $l14 i32) (local $l15 i32) (local $l16 i32) (local $l17 i32) (local $l18 i32) (local $l19 i32) (local $l20 i32) (local $l21 i32) (local $l22 i32) (local $l23 i32)
    (local.set $l1
      (global.get $__stack_pointer))
    (local.set $l2
      (i32.const 16))
    (local.set $l3
      (i32.sub
        (local.get $l1)
        (local.get $l2)))
    (global.set $__stack_pointer
      (local.get $l3))
    (i32.store offset=8
      (local.get $l3)
      (local.get $p0))
    (local.set $l4
      (i32.load offset=8
        (local.get $l3)))
    (local.set $l5
      (i32.const 2))
    (local.set $l6
      (local.get $l4))
    (local.set $l7
      (local.get $l5))
    (local.set $l8
      (i32.lt_s
        (local.get $l6)
        (local.get $l7)))
    (local.set $l9
      (i32.const 1))
    (local.set $l10
      (i32.and
        (local.get $l8)
        (local.get $l9)))
    (block $B0
      (block $B1
        (br_if $B1
          (i32.eqz
            (local.get $l10)))
        (local.set $l11
          (i32.const 1))
        (i32.store offset=12
          (local.get $l3)
          (local.get $l11))
        (br $B0))
      (local.set $l12
        (i32.load offset=8
          (local.get $l3)))
      (local.set $l13
        (i32.const 2))
      (local.set $l14
        (i32.sub
          (local.get $l12)
          (local.get $l13)))
      (local.set $l15
        (call $fib
          (local.get $l14)))
      (local.set $l16
        (i32.load offset=8
          (local.get $l3)))
      (local.set $l17
        (i32.const 1))
      (local.set $l18
        (i32.sub
          (local.get $l16)
          (local.get $l17)))
      (local.set $l19
        (call $fib
          (local.get $l18)))
      (local.set $l20
        (i32.add
          (local.get $l15)
          (local.get $l19)))
      (i32.store offset=12
        (local.get $l3)
        (local.get $l20)))
    (local.set $l21
      (i32.load offset=12
        (local.get $l3)))
    (local.set $l22
      (i32.const 16))
    (local.set $l23
      (i32.add
        (local.get $l3)
        (local.get $l22)))
    (global.set $__stack_pointer
      (local.get $l23))
    (return
      (local.get $l21)))
  (memory $memory (export "memory") 2)
  (global $__stack_pointer (mut i32) (i32.const 66560))
  (global $__dso_handle (export "__dso_handle") i32 (i32.const 1024))
  (global $__data_end (export "__data_end") i32 (i32.const 1024))
  (global $__stack_low (export "__stack_low") i32 (i32.const 1024))
  (global $__stack_high (export "__stack_high") i32 (i32.const 66560))
  (global $__global_base (export "__global_base") i32 (i32.const 1024))
  (global $__heap_base (export "__heap_base") i32 (i32.const 66560))
  (global $__heap_end (export "__heap_end") i32 (i32.const 131072))
  (global $__memory_base (export "__memory_base") i32 (i32.const 0))
  (global $__table_base (export "__table_base") i32 (i32.const 1)))
        )WAT");
  auto store = engine->create_store();
  auto instance = store->create_instance(*engine, *loop_module);
  auto func_handle = instance->lookup_function(*store, "fib");
  auto running_func = func_handle->invoke(*store, 32);
  bool done = false;
  // A busy loop where we poll the WASM VM to see if it's completed it's work
  // Make sure to yield so the seastar scheduler can do other work.
  while (!done) {
    done = running_func->pump() != -1;
  }
}
}

int main(int argc, char **argv) {
  auto Start = std::chrono::system_clock::now();

  run_fib_native();
  auto Step = std::chrono::system_clock::now();
  std::chrono::duration<double> DiffNative = Step - Start;
  std::cout << "run native fib(32), ints : " << DiffNative.count() << " s\n";

  run_fib_wasm();
  auto End = std::chrono::system_clock::now();
  std::chrono::duration<double> DiffWasm = End - Step;
  std::cout << "run wasm fib(32), ints : " << DiffWasm.count() << " s\n";

  run_fib_wasmtime();
  auto End2 = std::chrono::system_clock::now();
  std::chrono::duration<double> DiffWasmtime = End2 - End;
  std::cout << "run wasm fib(32), ints : " << DiffWasmtime.count() << " s\n";

  return 0;
}
