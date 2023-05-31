cmake_minimum_required(VERSION 3.14)

# Set your project name here
project(fastgron VERSION 0.4.8 LANGUAGES CXX)

set(FASTGRON_VERSION "${fastgron_VERSION}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/version.h)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g")

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
