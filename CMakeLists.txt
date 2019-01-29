cmake_minimum_required(VERSION 3.7)
project(UDPConnection)

set(UDPConnection_SOURCES
    src/UDPConnection.c
    src/UDPC_Deque.c
    src/UDPC_Defines.c
)

set(CMAKE_C_FLAGS "-Wall -Wno-missing-braces")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
set(CMAKE_C_FLAGS_RELEASE "-O3 -D NDEBUG")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug', none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

add_library(UDPConnection ${UDPConnection_SOURCES})

target_compile_features(UDPConnection PUBLIC c_std_11)
target_link_libraries(UDPConnection PUBLIC pthread)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(UDPC_UnitTest_SOURCES
        src/test/UDPC_UnitTest.c)
    add_executable(UnitTest ${UDPC_UnitTest_SOURCES})
    target_link_libraries(UnitTest PUBLIC UDPConnection)
    target_include_directories(UnitTest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()
