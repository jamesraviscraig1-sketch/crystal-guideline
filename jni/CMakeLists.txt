cmake_minimum_required(VERSION 3.18)
project(crystal_guideline)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fvisibility=hidden -fPIC -Wall")

add_library(crystal_guideline SHARED
    main.cpp
    # All other .hpp are header-only — no separate .cpp needed
)

target_include_directories(crystal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_library(log-lib  log)
find_library(android-lib android)

target_link_libraries(crystal_guideline
    ${log-lib}
    ${android-lib}
    dl
)
