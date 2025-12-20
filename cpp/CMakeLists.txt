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

add_executable(replication_tests tests/replication_tests.cpp tests/replication_simulator_tests.cpp tests/replication_failover_tests.cpp)
target_link_libraries(replication_tests gtest_main distributed_cache_lib)
add_test(NAME replication_tests COMMAND replication_tests)

# Raft unit tests
add_executable(raft_tests tests/raft_tests.cpp tests/raft_wal_tests.cpp tests/raft_integration_tests.cpp tests/raft_snapshot_tests.cpp tests/raft_install_snapshot_tests.cpp tests/raft_snapshot_creation_tests.cpp tests/raft_restart_snapshot_tests.cpp)
target_link_libraries(raft_tests gtest_main distributed_cache_lib)
add_test(NAME raft_tests COMMAND raft_tests)

add_executable(sharder_tests tests/sharder_tests.cpp)
target_link_libraries(sharder_tests gtest_main distributed_cache_lib)
add_test(NAME sharder_tests COMMAND sharder_tests)

add_executable(rebalancer_tests tests/sharder_rebalance_tests.cpp)
target_link_libraries(rebalancer_tests gtest_main distributed_cache_lib)
add_test(NAME rebalancer_tests COMMAND rebalancer_tests)

add_executable(rebalancer_more_tests tests/sharder_rebalance_more_tests.cpp)
target_link_libraries(rebalancer_more_tests gtest_main distributed_cache_lib)
add_test(NAME rebalancer_more_tests COMMAND rebalancer_more_tests)

# Rebalancer benchmark
add_executable(rebalancer_bench benchmarks/rebalancer_bench.cpp)
target_link_libraries(rebalancer_bench distributed_cache_lib)

# Benchmarks
add_executable(perf_bench benchmarks/perf_bench.cpp)
target_link_libraries(perf_bench distributed_cache_lib)