cmake_minimum_required(VERSION 3.7)
project(UDPC)

set(UDPC_VERSION 1.2)
set(UDPC_SOVERSION 1)

set(UDPC_SOURCES
    src/UDPConnection.cpp
    src/CXX11_shared_spin_lock.cpp
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

# Enforce linking with libstdc++ even with C targets.
# Doesn't affect conan, so conanfile.py was also modified for this.
if(APPLE)
set_target_properties(UDPC PROPERTIES
    INTERFACE_LINK_LIBRARIES "libc++"
)
elseif(UNIX)
set_target_properties(UDPC PROPERTIES
    INTERFACE_LINK_LIBRARIES "stdc++"
)
endif()

set_target_properties(UDPC PROPERTIES VERSION ${UDPC_VERSION} SOVERSION ${UDPC_SOVERSION})

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

find_package(libsodium REQUIRED)
target_compile_definitions(UDPC PUBLIC UDPC_LIBSODIUM_ENABLED)
target_link_libraries(UDPC PUBLIC libsodium::libsodium)


if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(UDPC_UnitTest_SOURCES
        src/CXX11_shared_spin_lock.cpp
        src/test/UDPC_UnitTest.cpp
        src/test/TestTSLQueue.cpp
        src/test/TestUDPC.cpp
        src/test/TestSharedSpinLock.cpp
    )
    add_executable(UnitTest ${UDPC_UnitTest_SOURCES})
    target_compile_features(UnitTest PUBLIC cxx_std_11)
    target_link_libraries(UnitTest PUBLIC UDPC)
    target_include_directories(UnitTest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(UnitTest PRIVATE "-Wno-sign-compare")

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
