cmake_minimum_required(VERSION 3.3)
project(OS_Projekt_WTFS)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14 -Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE")

set(SOURCE_FILES
    common.hpp
    main.cpp
    ops.cpp
    ops.hpp
    structure.cpp
    structure.hpp
    tests.cpp
    tests.hpp)

add_executable(OS_Projekt_WTFS ${SOURCE_FILES})

target_link_libraries(OS_Projekt_WTFS
        PUBLIC fuse)
