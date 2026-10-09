cmake_minimum_required(VERSION 3.20)
project(crivo_ctest_timeout_fixture NONE)
enable_testing()
add_test(NAME synthetic_timeout COMMAND ${CMAKE_COMMAND} -E sleep 5)
