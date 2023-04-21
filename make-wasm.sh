clang-16 \
  --target=wasm32 \
  -nostdlib \
  -Wl,--no-entry \
  -Wl,--export-all \
  -o fib.wasm \
  fib.c
