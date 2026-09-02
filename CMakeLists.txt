cmake_minimum_required(VERSION 3.22)

project(Columnar-Engine-From-Scratch LANGUAGES CXX)

include(FetchContent)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -O3")
option(
  COLUMNAR_ENABLE_NATIVE_OPTIMIZATIONS
  "Build with -march=native for the CPU of the build machine"
  OFF
)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe")
if(COLUMNAR_ENABLE_NATIVE_OPTIMIZATIONS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
add_compile_options($<$<CONFIG:Release>:-O2>)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

add_library(
  columnar_engine
  STATIC
  src/csv_wrapper/csv_wrapper.cpp
  src/column_types/column_types.cpp
  src/utilities/utilities.cpp
  src/file_writer/file_writer.cpp
  src/file_reader/file_reader.cpp
  src/scheme/scheme.cpp
  src/operators/operators.cpp
)
target_include_directories(
  columnar_engine
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(
  benchmark_application
  STATIC
  src/benchmark/benchmark_service.cpp
  src/benchmark/clickbench_dataset.cpp
  src/benchmark/clickbench_queries.cpp
)
target_link_libraries(
  benchmark_application
  PUBLIC
  columnar_engine
)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
  cpp_httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.51.0.tar.gz
  URL_HASH SHA256=d740ced75352f44e9d66d08806dc231b5621bb3592b5a5d5b2bd890a9a9d86cc
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cpp_httplib)

add_executable(
  benchmark-service
  src/server/benchmark_http_server.cpp
  src/server/main.cpp
)
target_link_libraries(
  benchmark-service
  PRIVATE
  benchmark_application
  httplib::httplib
  nlohmann_json::nlohmann_json
  Threads::Threads
)

include(CTest)

if(BUILD_TESTING)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(
    tests
    test.cpp
  )
  add_executable(
    benchmark
    benchmark_test.cpp
  )
  target_link_libraries(
    tests
    PRIVATE
    benchmark_application
    GTest::gtest_main
  )
  target_link_libraries(
    benchmark
    PRIVATE
    benchmark_application
    GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(tests)
  gtest_discover_tests(benchmark)
endif()
