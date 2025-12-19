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