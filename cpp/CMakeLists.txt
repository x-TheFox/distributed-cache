cmake_minimum_required(VERSION 3.10)

project(DistributedCache)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB_RECURSE ALL_SOURCES
    src/*.cpp
)

# Separate main from library sources so we can link tests against the core library
list(FILTER ALL_SOURCES EXCLUDE REGEX "src/main.cpp$")
set(LIB_SOURCES ${ALL_SOURCES})

add_library(distributed_cache_lib STATIC ${LIB_SOURCES})
add_executable(distributed_cache src/main.cpp)
target_link_libraries(distributed_cache distributed_cache_lib)

# Add any necessary libraries here for distributed_cache
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
target_link_libraries(cache_tests gtest_main distributed_cache_lib)
add_test(NAME cache_tests COMMAND cache_tests)