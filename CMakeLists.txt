cmake_minimum_required(VERSION 3.15)

project(silentot_docker)

find_package(libOTe REQUIRED)

add_executable(main source/main.cpp)

target_compile_options(main PUBLIC 
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17> -lpthread)

target_include_directories(main PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    include)

target_link_libraries(main oc::libOTe)