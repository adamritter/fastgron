cmake_minimum_required(VERSION 3.14)

# Set your project name here
project(fastgron VERSION 0.4.14 LANGUAGES CXX)

set(FASTGRON_VERSION "${fastgron_VERSION}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/version.h)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g $ENV{CXX_ARCH_FLAGS}")

# Include your source files here
add_executable(fastgron
    src/fastgron.cpp
    extern/simdjson/simdjson.cpp
)
target_include_directories(fastgron PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include extern/simdjson)

find_package(CURL)

# Link dependencies
target_link_libraries(fastgron PRIVATE)

# Tell compiler about CURL_FOUND
if(CURL_FOUND)
    target_compile_definitions(fastgron PRIVATE CURL_FOUND)
    target_link_libraries(fastgron PRIVATE ${CURL_LIBRARIES})
endif()

install(TARGETS fastgron
    RUNTIME DESTINATION bin) # for executables

# use ctest
include(CTest)

# Add test
enable_testing()

# Use bash for testing
find_program(BASH_PROGRAM bash)

if(BASH_PROGRAM)
    add_test(version_test ${BASH_PROGRAM} -c "${CMAKE_CURRENT_BINARY_DIR}/fastgron -V 2>&1")
    set_tests_properties(version_test PROPERTIES PASS_REGULAR_EXPRESSION "fastgron version ${FASTGRON_VERSION}")

    add_test(array_test ${BASH_PROGRAM} -c "echo '{\"a\":[1,2,3]}' | ${CMAKE_CURRENT_BINARY_DIR}/fastgron")
    set_tests_properties(array_test PROPERTIES PASS_REGULAR_EXPRESSION "json.a\\[1\\] = 2\njson.a\\[2\\] = 3")

    add_test(map_test ${BASH_PROGRAM} -c "echo '{\"a\":{\"c\":1,\"b\":2}}' | ${CMAKE_CURRENT_BINARY_DIR}/fastgron")
    set_tests_properties(map_test PROPERTIES PASS_REGULAR_EXPRESSION "json.a.c = 1\njson.a.b = 2")

    add_test(sort_map_test ${BASH_PROGRAM} -c "echo '{\"a\":{\"c\":1,\"b\":2}}' | ${CMAKE_CURRENT_BINARY_DIR}/fastgron --sort")
    set_tests_properties(sort_map_test PROPERTIES PASS_REGULAR_EXPRESSION "json.a.b = 2\njson.a.c = 1")

    add_test(filter_fixed_string_text ${BASH_PROGRAM} -c "echo '{\"a\":[5,4,3,1]}' | ${CMAKE_CURRENT_BINARY_DIR}/fastgron -F 1")
    set_tests_properties(filter_fixed_string_text PROPERTIES PASS_REGULAR_EXPRESSION "json.a\\[1\\] = 4\njson.a\\[3\\] = 1")

    add_test(sort_and_filter_test ${BASH_PROGRAM} -c "echo '{\"a\":{\"c\":13,\"b\":22,\"a\":31}}' | ${CMAKE_CURRENT_BINARY_DIR}/fastgron --sort -F 3")
    set_tests_properties(sort_and_filter_test PROPERTIES PASS_REGULAR_EXPRESSION "json.a.a = 31\njson.a.c = 13")

    add_test(root_test ${BASH_PROGRAM} -c "echo '[1,2,3]' | ${CMAKE_CURRENT_BINARY_DIR}/fastgron --root a")
    set_tests_properties(root_test PROPERTIES PASS_REGULAR_EXPRESSION "a\\[1\\] = 2\na\\[2\\] = 3")

    add_test(array_path_test ${BASH_PROGRAM} -c "echo '[3,4,5]' | ${CMAKE_CURRENT_BINARY_DIR}/fastgron .1")
    set_tests_properties(array_path_test PROPERTIES PASS_REGULAR_EXPRESSION "json\\[1\\] = 4")
endif(BASH_PROGRAM)
