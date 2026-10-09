cmake_minimum_required(VERSION 3.20)
project(crivo_qualification_fixture LANGUAGES NONE)
enable_testing()
add_test(NAME fixture_smoke COMMAND ${CMAKE_COMMAND} -E true)
