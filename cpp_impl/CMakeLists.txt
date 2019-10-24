cmake_minimum_required(VERSION 3.7)
project(UDPConnection)

set(UDPConnection_VERSION 1.0)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/RingBuffer/src)
    message(FATAL_ERROR "RingBuffer is missing!\nPlease update the \
RingBuffer submodule by running 'git submodule init' and 'git submodule \
update'!")
endif()

set(UDPConnection_SOURCES
    src/UDPConnection.cpp
)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wno-missing-braces")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -D NDEBUG")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug', none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

add_library(UDPConnection ${UDPConnection_SOURCES})

set_target_properties(UDPConnection PROPERTIES VERSION ${UDPConnection_VERSION})

target_compile_features(UDPConnection PUBLIC cxx_std_17)
target_link_libraries(UDPConnection PUBLIC pthread)
if(WIN32)
    if(MINGW)
        target_compile_definitions(UDPConnection PUBLIC UDPC_PLATFORM_MINGW)
        target_link_libraries(UDPConnection PUBLIC ws2_32)
        target_link_libraries(UDPConnection PUBLIC iphlpapi)
    else()
        target_link_libraries(UDPConnection PUBLIC Ws2_32)
        target_link_libraries(UDPConnection PUBLIC Iphlpapi)
    endif()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSODIUM REQUIRED libsodium)
target_link_libraries(UDPConnection PUBLIC ${LIBSODIUM_LIBRARIES})
target_include_directories(UDPConnection PUBLIC ${LIBSODIUM_INCLUDE_DIRS})
target_compile_options(UDPConnection PUBLIC ${LIBSODIUM_CFLAGS_OTHER})

target_include_directories(UDPConnection PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/RingBuffer/src")

if(CMAKE_BUILD_TYPE MATCHES "Debug")

    find_package(GTest QUIET)
    if(GTEST_FOUND)
        set(UDPC_UnitTest_SOURCES
            src/test/UDPC_UnitTest.cpp
            src/test/TestTSQueue.cpp
            src/test/TestTSLQueue.cpp
            src/test/TestUDPC.cpp
        )
        add_executable(UnitTest ${UDPC_UnitTest_SOURCES})
        target_compile_features(UnitTest PUBLIC cxx_std_17)
        target_link_libraries(UnitTest PUBLIC UDPConnection ${GTEST_BOTH_LIBRARIES})
        target_include_directories(UnitTest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    endif()

    set(UDPC_NetworkTest_SOURCES
        src/test/UDPC_NetworkTest.cpp)
    add_executable(NetworkTest ${UDPC_NetworkTest_SOURCES})
    target_link_libraries(NetworkTest PUBLIC UDPConnection)
    target_include_directories(NetworkTest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

install(TARGETS UDPConnection DESTINATION lib)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UDPConnection.h
    DESTINATION include)
