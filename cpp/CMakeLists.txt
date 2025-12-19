cmake_minimum_required(VERSION 3.10)

project(DistributedCache)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(distributed_cache ${SOURCES})

# Add any necessary libraries here
# target_link_libraries(distributed_cache <library_name>)

# Enable testing and add unit tests
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(cache_tests tests/cache_tests.cpp)
target_link_libraries(cache_tests gtest_main)
add_test(NAME cache_tests COMMAND cache_tests)