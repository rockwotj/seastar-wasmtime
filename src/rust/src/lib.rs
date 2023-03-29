use futures::future::BoxFuture;
use futures::Future;
use core::task::Poll;
use core::pin::Pin;
use core::task::Context;
use wasmtime::Result;

#[cxx::bridge(namespace = "wasm")]
mod bridge {
    extern "Rust" {
        type Engine;
        unsafe fn create_engine() -> Result<Box<Engine>>;
        type Module;
        unsafe fn compile_module(self: &mut Engine, wat: &str) -> Result<Box<Module>>;
        type Store;
        unsafe fn create_store(self: &mut Engine) -> Result<Box<Store>>;
        type Instance;
        unsafe fn create_instance(self: &mut Store, engine: &Engine, module: &Module) -> Result<Box<Instance>>;
        type FunctionHandle;
        unsafe fn lookup_function(self: &mut Instance, store: &mut Store, func_name: &str) -> Result<Box<FunctionHandle>>;
        unsafe fn invoke<'a>(self: &'a mut FunctionHandle, store: &'a mut Store) -> Box<RunningFunction<'a>>;
        type RunningFunction<'a>;
        // Pump the function and return if it finished or not
        unsafe fn pump(self: &mut RunningFunction<'_>) -> Result<bool>;
    }
    unsafe extern "C++" {
        include!("seastar_ffi.h");
        type SeastarVoidFuture;
        fn poll_seastar_future(future: &UniquePtr<SeastarVoidFuture>) -> Result<bool>;
        fn seastar_sleep() -> UniquePtr<SeastarVoidFuture>;
    }
}
unsafe impl Send for bridge::SeastarVoidFuture {}

pub struct Engine {
    engine: wasmtime::Engine,
}

fn create_engine() -> Result<Box<Engine>> {
    let mut config = wasmtime::Config::new();
    config.async_support(true);
    config.consume_fuel(true);
    // TODO: Use seastar malloc for wasm memory
    // This tells wasmtime to have dynamic memory usage of just what is needed.
    config.static_memory_maximum_size(0);
    config.dynamic_memory_reserved_for_growth(0);
    config.dynamic_memory_guard_size(0);
    config.max_wasm_stack(128 * 1024);
    config.async_stack_size(256 * 1024);
    let engine = wasmtime::Engine::new(&config)?;
    return Ok(Box::new(Engine { engine }));
}

impl Engine {
        fn compile_module(self: &mut Engine, wat: &str) -> Result<Box<Module>> {
            return Ok(Box::new(Module { module: wasmtime::Module::new(&self.engine, wat)? }));
        }

        fn create_store(self: &mut Engine) -> Result<Box<Store>> {
            let mut store = wasmtime::Store::new(&self.engine, ());
            // If you want to see what happens when fuel is exhausted make these numbers really low.
            store.out_of_fuel_async_yield(/*injection_count=*/100_000_000_000, /*fuel_to_inject=*/100_000_000);
            return Ok(Box::new(Store { store }))
        }
}

pub struct Module {
    module: wasmtime::Module,
}

pub struct Store {
    store: wasmtime::Store<()>,
}

pub struct SeastarFutureAdapter {
    inner: cxx::UniquePtr<bridge::SeastarVoidFuture>,
}

impl Future for SeastarFutureAdapter {
    type Output = Result<()>;
    fn poll(self: Pin<&mut Self>, _cx: &mut Context) -> Poll<Self::Output> {
        match bridge::poll_seastar_future(&self.inner) {
            Ok(true) => Poll::Ready(Ok(())),
            Ok(false) => Poll::Pending,
            Err(x) => Poll::Ready(Err(x.into()))
        }
    }
}



fn async_sleep() -> Box<dyn Future<Output = Result<()>> + std::marker::Send> {
    Box::new(SeastarFutureAdapter { inner: bridge::seastar_sleep() }) 
}

impl Store {
    fn create_instance(self: &mut Store, engine: &Engine, module: &Module) -> Result<Box<Instance>> {
        let mut linker = wasmtime::Linker::new(&engine.engine);
        linker.func_new_async(
            "host",
            "sleep",
            wasmtime::FuncType::new(None, None),
            |_caller, _params, _results| async_sleep(),
        )?;
        let mut instance_future = Box::pin(linker.instantiate_async(&mut self.store, &module.module));
        // TODO: Make creating an engine async too, until then, just poll until completion
        let mut noop_waker = core::task::Context::from_waker(futures::task::noop_waker_ref());
        loop {
            match instance_future.as_mut().poll(&mut noop_waker) {
                Poll::Pending => {}
                Poll::Ready(Ok(instance)) => {
                    return Ok(Box::new(Instance { instance }))
                }
                Poll::Ready(Err(e)) => {
                    return Err(e);
                }
            }
        }
    }
}

pub struct Instance {
    instance: wasmtime::Instance,
}

impl Instance {
    fn lookup_function(self: &mut Instance, store: &mut Store, func_name: &str) -> Result<Box<FunctionHandle>> {
        let handle = self.instance.get_typed_func::<(), ()>(&mut store.store, &func_name).unwrap();
        return Ok(Box::new(FunctionHandle { 
            handle
        }));
    }
}

pub struct FunctionHandle {
    handle: wasmtime::TypedFunc<(), ()>,
}

impl FunctionHandle {
    fn invoke<'a>(self: &'a mut FunctionHandle, store: &'a mut Store) -> Box<RunningFunction<'a>> {
        return Box::new(
            RunningFunction {
                fut: Box::pin(self.handle.call_async(&mut store.store, ())) 
            }
        )
    }
}

pub struct RunningFunction<'a> {
    fut: BoxFuture<'a, Result<()>>,
}

impl<'a> RunningFunction<'a> {
    fn pump(&mut self) -> Result<bool> {
        let mut noop_waker = core::task::Context::from_waker(futures::task::noop_waker_ref());
        match self.fut.as_mut().poll(&mut noop_waker) {
            Poll::Pending => Ok(false),
            Poll::Ready(Ok(())) => Ok(true),
            Poll::Ready(Err(e)) => Err(e),
        }
    }
}
