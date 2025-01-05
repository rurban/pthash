cmake_minimum_required(VERSION 3.5)
project(PTHASH)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif ()
MESSAGE(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

if(NOT TARGET PTHASH)
  add_library(PTHASH INTERFACE)
  target_include_directories(PTHASH INTERFACE ./include)
  target_include_directories(PTHASH INTERFACE .)

  MESSAGE(STATUS "Compiler " ${CMAKE_CXX_COMPILER_ID} " for processor " ${CMAKE_HOST_SYSTEM_PROCESSOR})
  target_compile_features(PTHASH INTERFACE cxx_std_17)
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    MESSAGE(STATUS "Compiling with flags: -stdlib=libc++ -Wno-unknown-attributes")
    add_compile_options(-stdlib=libc++ -Wno-unknown-attributes)
  endif()

  if (UNIX AND (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64"))
    MESSAGE(STATUS "Compiling with flags: -march=native -mbmi2 -msse4.2")
    add_compile_options(-march=native)
    # Flags for PTHASH:
    add_compile_options(-mbmi2 -msse4.2) # for hardware popcount and pdep
  endif()

  if (PTHASH_ENABLE_LARGE_BUCKET_ID_TYPE)
    MESSAGE(STATUS "bucket_id_type is uint64_t")
    add_compile_options(-DPTHASH_ENABLE_LARGE_BUCKET_ID_TYPE)
  endif()

  if (UNIX)
    add_compile_options(-pthread)
    add_compile_options(-ggdb)

    MESSAGE(STATUS "Compiling with flags: -std=c++17 -ggdb -pthread -Wall -Wextra -Wno-missing-braces  -Wno-unused-function")
    add_compile_options(-Wall -Wextra -Wno-missing-braces -Wno-unused-function)

    if (PTHASH_USE_SANITIZERS)
      set(SAN_FLAGS -fsanitize=address -fno-omit-frame-pointer -fdiagnostics-all-candidates)
      MESSAGE(STATUS "Using sanitizers. Compiling with flags: ${SAN_FLAGS}")
      add_compile_options(${SAN_FLAGS})
      add_link_options(${SAN_FLAGS})
    endif()
  endif()

  add_subdirectory(external/essentials)
  target_link_libraries(PTHASH INTERFACE ESSENTIALS)

  target_include_directories(PTHASH SYSTEM INTERFACE external/xxHash)
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
    add_test(${TEST_SRC_NAME} ${TEST_SRC_NAME})
  endforeach(TEST_SRC)
  add_test(example-inc ./example-inc)
endif()
