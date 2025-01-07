cmake_minimum_required(VERSION 3.5)
project(PTHASH)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif ()
MESSAGE(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

if(NOT TARGET PTHASH)
  add_library(PTHASH INTERFACE)
  target_include_directories(PTHASH INTERFACE include .)

  target_compile_features(PTHASH INTERFACE cxx_std_17)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    MESSAGE(STATUS "Using clang++. Compiling with flags: -stdlib=libc++ -Wno-unknown-attributes")
    add_compile_options(-stdlib=libc++ -Wno-unknown-attributes)
    add_link_options(-stdlib=libc++)
  endif()

  MESSAGE(STATUS "Compiler " ${CMAKE_CXX_COMPILER_ID} " for processor: " ${CMAKE_HOST_SYSTEM_PROCESSOR})

  if (UNIX AND (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64"))
    MESSAGE(STATUS "Compiling with flags: -march=native -mbmi2 -msse4.2")
    add_compile_options(-march=native)
    add_compile_options(-mbmi2 -msse4.2) # for hardware popcount and pdep
  endif()

  option(PTHASH_ENABLE_ALL_ENCODERS "Compiling with all encoders" FALSE)
  if (PTHASH_ENABLE_ALL_ENCODERS)
    MESSAGE(STATUS "Compiling with all encoders")
    add_compile_options(-DPTHASH_ENABLE_ALL_ENCODERS)
  endif()

  option(PTHASH_ENABLE_LARGE_BUCKET_ID_TYPE "Set bucket_id_type to uint64_t" FALSE)
  if (PTHASH_ENABLE_LARGE_BUCKET_ID_TYPE)
    MESSAGE(STATUS "Compiling with bucket_id_type as uint64_t")
    add_compile_options(-DPTHASH_ENABLE_LARGE_BUCKET_ID_TYPE)
  endif()

  find_package(Threads REQUIRED)
  target_link_libraries(PTHASH INTERFACE Threads::Threads)

  option(PTHASH_USE_SANITIZERS "Use asan,ubsan" FALSE)
  if (PTHASH_USE_SANITIZERS)
    MESSAGE(STATUS "Using sanitizers. Compiling with flags: -fsanitize=address,undefined -fno-omit-frame-pointer")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fdiagnostics-all-candidates)
    add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  endif()

  MESSAGE(STATUS "Compiling with flags: -std=c++17 -pthread -Wall -Wextra -Wno-missing-braces -Wno-unused-function")
  add_compile_options(-Wall -Wextra -Wno-missing-braces -Wno-unused-function)

  add_subdirectory(external/bits)
  target_link_libraries(PTHASH INTERFACE BITS)
endif()

# Only add benchmarks and tests when compiling PTHash itself, not when added as a dependency
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  add_executable(build src/build.cpp)
  target_link_libraries(build PRIVATE PTHASH)
  add_executable(example src/example.cpp)
  target_link_libraries(example PRIVATE PTHASH)

  add_executable(example-c src/example-c.cpp)
  target_link_libraries(example-c PRIVATE PTHASH)
  add_custom_command(OUTPUT pthash-example.hpp
    COMMAND ./example-c
    DEPENDS example-c)
  add_executable(example-inc src/example-inc.cpp pthash-example.hpp)
  target_link_libraries(example-inc PRIVATE PTHASH)

  enable_testing()
  file(GLOB TEST_SOURCES test/test_*.cpp)
  foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component (TEST_SRC_NAME ${TEST_SRC} NAME_WE) # without extension
    add_executable(${TEST_SRC_NAME} ${TEST_SRC})
    add_test(${TEST_SRC_NAME} ${TEST_SRC_NAME})
    target_link_libraries(${TEST_SRC_NAME} PRIVATE PTHASH)
  endforeach(TEST_SRC)
  add_test(example-inc ./example-inc)
endif()
