add_library(cache_tests cache_tests.cpp)

target_link_libraries(cache_tests PRIVATE gtest gtest_main cache)

include(GoogleTest)
gtest_discover_tests(cache_tests)