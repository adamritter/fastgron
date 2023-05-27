cmake_minimum_required(VERSION 3.14)

# Set your project name here
project(fastgron VERSION 1.0.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Include your source files here
add_executable(fastgron
    src/fastgron.cpp
    extern/simdjson/simdjson.cpp
)
target_include_directories(fastgron PRIVATE extern/simdjson extern/fast_io)

# Link dependencies
target_link_libraries(fastgron PRIVATE)

install(TARGETS fastgron
    RUNTIME DESTINATION bin) # for executables
