cmake_minimum_required(VERSION 3.7)
project(UDPC)

set(UDPC_VERSION 1.0)

set(UDPC_SOURCES
    src/UDPConnection.cpp
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

add_library(UDPC ${UDPC_SOURCES})

set_target_properties(UDPC PROPERTIES VERSION ${UDPC_VERSION})

target_compile_features(UDPC PUBLIC cxx_std_11)
target_link_libraries(UDPC PUBLIC pthread)
if(WIN32)
    if(MINGW)
        target_compile_definitions(UDPC PUBLIC UDPC_PLATFORM_MINGW)
        target_link_libraries(UDPC PUBLIC ws2_32)
        target_link_libraries(UDPC PUBLIC iphlpapi)
    else()
        target_link_libraries(UDPC PUBLIC Ws2_32)
        target_link_libraries(UDPC PUBLIC Iphlpapi)
    endif()
endif()

if(UDPC_DISABLE_LIBSODIUM)
    message(STATUS "libsodium disabled")
elseif(DEFINED M_LIBSODIUM_LIBRARIES AND DEFINED M_LIBSODIUM_INCLUDE_DIRS)
    message(STATUS "libsodium manual paths detected, using them")
    target_compile_definitions(UDPC PUBLIC UDPC_LIBSODIUM_ENABLED)
    target_link_libraries(UDPC PUBLIC ${M_LIBSODIUM_LIBRARIES})
    target_include_directories(UDPC PUBLIC ${M_LIBSODIUM_INCLUDE_DIRS})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSODIUM QUIET libsodium)
    if(LIBSODIUM_FOUND)
        target_compile_definitions(UDPC PUBLIC UDPC_LIBSODIUM_ENABLED)
        target_link_libraries(UDPC PUBLIC ${LIBSODIUM_LIBRARIES})
        target_include_directories(UDPC PUBLIC ${LIBSODIUM_INCLUDE_DIRS})
        target_compile_options(UDPC PUBLIC ${LIBSODIUM_CFLAGS_OTHER})
        message(STATUS "libsodium enabled")
    else()
        message(STATUS "libsodium not found, UDPC will be compiled without libsodium support")
    endif()
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")

    find_package(GTest QUIET)
    if(GTEST_FOUND)
        set(UDPC_UnitTest_SOURCES
            src/test/UDPC_UnitTest.cpp
            src/test/TestTSLQueue.cpp
            src/test/TestUDPC.cpp
        )
        add_executable(UnitTest ${UDPC_UnitTest_SOURCES})
        target_compile_features(UnitTest PUBLIC cxx_std_11)
        target_link_libraries(UnitTest PUBLIC UDPC ${GTEST_BOTH_LIBRARIES})
        target_include_directories(UnitTest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    endif()

    set(UDPC_NetworkTest_SOURCES
        src/test/UDPC_NetworkTest.c)
    add_executable(NetworkTest ${UDPC_NetworkTest_SOURCES})
    target_link_libraries(NetworkTest PUBLIC UDPC)
    target_include_directories(NetworkTest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

install(TARGETS UDPC DESTINATION lib)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UDPC.h
    DESTINATION include)
