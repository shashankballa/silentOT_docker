cmake_minimum_required(VERSION 3.15)

project(silentot_docker)

# Find libOTe package
find_package(libOTe REQUIRED)

# Specify the executable and all of its source files
add_executable(main source/main.cpp include/ExConv_tests.cpp)

# Set compile options
# NB: REMOVE -g after debugging!
target_compile_options(main PUBLIC 
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -g> -lpthread)

# Include directories
target_include_directories(main PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    include)

# Link with libOTe
target_link_libraries(main oc::libOTe)
