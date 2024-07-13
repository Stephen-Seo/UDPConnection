cmake_minimum_required(VERSION 3.7)
project(UDPC_NetworkTest C)

set(UDPC_NetworkTest_SOURCES
    src/UDPC_NetworkTest.c
)

add_compile_options(
    -Wall -Wextra -Wpedantic -Wno-missing-braces
    $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>
    $<$<CONFIG:DEBUG>:-Og>
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug', none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

find_package(udpc CONFIG REQUIRED)

add_executable(UDPC_NetworkTest
    ${UDPC_NetworkTest_SOURCES}
)
target_link_libraries(UDPC_NetworkTest PUBLIC udpc::udpc)
