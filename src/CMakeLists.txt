cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)

corrosion_import_crate(MANIFEST_PATH rust/Cargo.toml)

corrosion_add_cxxbridge(cxxbridge-rust CRATE cxx-rust MANIFEST_PATH rust FILES lib.rs)
target_include_directories(cxxbridge-rust PUBLIC "include")
target_sources(cxxbridge-rust PRIVATE seastar_ffi.cc)
target_link_libraries(cxxbridge-rust PUBLIC Seastar::seastar)

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
  PRIVATE
  main.cc
)
target_link_libraries(${PROJECT_NAME} PUBLIC cxxbridge-rust Seastar::seastar)