cmake_minimum_required(VERSION 3.22)

project(Columnar-Engine-From-Scratch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -march=native -pipe")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
  tests
  test.cpp
  src/csv_wrapper/csv_wrapper.cpp
  src/column_types/column_types.cpp
  src/utilities/utilities.cpp
  src/file_writer/file_writer.cpp
  src/file_reader/file_reader.cpp
  src/scheme/scheme.cpp
  src/operators/operators.cpp
)
add_executable(
  benchmark
  benchmark_test.cpp
  src/csv_wrapper/csv_wrapper.cpp
  src/column_types/column_types.cpp
  src/utilities/utilities.cpp
  src/file_writer/file_writer.cpp
  src/file_reader/file_reader.cpp
  src/scheme/scheme.cpp
  src/operators/operators.cpp
)
target_link_libraries(
  tests
  GTest::gtest_main
)
target_link_libraries(
  benchmark
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(tests)
gtest_discover_tests(benchmark)

add_executable(main main.cpp)
