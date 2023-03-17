cmake_minimum_required(VERSION 3.15)

project(silentot_testing)

find_package(libOTe REQUIRED)

add_executable(main main.cpp)

target_compile_options(main PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++14> -lpthread)

target_include_directories(main PUBLIC
                          "${CMAKE_CURRENT_SOURCE_DIR}"
                          )

target_link_libraries(main oc::libOTe)