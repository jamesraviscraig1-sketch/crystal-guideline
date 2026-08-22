cmake_minimum_required(VERSION 3.22.1)
project(crystal_guideline CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Release optimizations + symbol stripping ──────────────────
add_compile_options(
    -O3
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -fno-rtti
    -fno-exceptions
)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections -Wl,--strip-all")

add_library(
    crystal_guideline
    SHARED
    main.cpp       # JNI entry + Canvas bridge
    # Headers-only: MathUtils.hpp Memory.hpp PatternScanner.hpp Guideline.hpp
)

find_library(LOG_LIB   log)
find_library(ANDROID_LIB android)

target_link_libraries(
    crystal_guideline
    ${LOG_LIB}
    ${ANDROID_LIB}
    -Wl,--gc-sections
)

target_include_directories(crystal_guideline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
