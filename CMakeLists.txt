cmake_minimum_required(VERSION 3.28)

project(
    SIPLOG
    VERSION 1.2.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(SIPLOG_LIBRARY siplog)
set(SIPLOG_DEBUG_LIBRARY siplog_debug)

option(ENABLE_TEST "enable building test exucutable" OFF)

if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_C_COMPILER_ID}" MATCHES "GNU")
    # common compiling options
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -fno-strict-aliasing
        -pthread
        -fstack-protector-strong
        -fno-short-enums
    )

    # options for Debug
    add_compile_options($<$<CONFIG:Debug>:-fno-omit-frame-pointer>)

    # options for Release
    add_compile_options($<$<CONFIG:Release>:-ffunction-sections>)
    add_compile_options($<$<CONFIG:Release>:-fdata-sections>)

    # options for RelWithDebInfo
    add_compile_options($<$<CONFIG:RelWithDebInfo>:-ffunction-sections>)
    add_compile_options($<$<CONFIG:RelWithDebInfo>:-fdata-sections>)
else()
    message(FATAL_ERROR "Not supported C Compiler: " ${CMAKE_C_COMPILER_ID})
endif()

add_library(${SIPLOG_LIBRARY} siplog.c siplog_logfile_async.c)
add_library(${SIPLOG_DEBUG_LIBRARY} siplog.c siplog_logfile_async.c siplog_mem_debug.c)

if(${ENABLE_TEST})
    add_executable(test test.c)
    target_link_libraries(test ${SIPLOG_DEBUG_LIBRARY})
endif()
