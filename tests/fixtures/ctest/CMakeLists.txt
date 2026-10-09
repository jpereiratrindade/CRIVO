cmake_minimum_required(VERSION 3.30)
project(crivo_synthetic_fixture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_test(NAME synthetic_pass COMMAND ${CMAKE_COMMAND} -E echo "Synthetic PASS test execution")
add_test(NAME synthetic_fail COMMAND ${CMAKE_COMMAND} -E false)
add_test(NAME synthetic_skip COMMAND ${CMAKE_COMMAND} -E echo "Synthetic SKIP test")
set_tests_properties(synthetic_skip PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP")
