# UDPConnection

Provides a network connection over UDP, with verification of packet support via
libsodium (optional). Implemented in C++ (up to C++11 standard), but is
available via a C api, which should facilitate creating bindings for other
programming languages if needed.

This library is still a work in progress, so api breaking changes may happen
in the future.

## Documentation

`src/UDPC.h` is documented with Doxygen style comments. The doxygen docs can be
created by invoking `doxygen Doxyfile` in the root directory of the project.

## Compiling

### Release builds

    mkdir buildRelease
    cd buildRelease
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=True ..
    make
    make DESTDIR=install_destination install

### Debug builds

    mkdir buildDebug
    cd buildDebug
    cmake -DCMAKE_BUILD_TYPE=Debug ..
    make

## Usage

The program in `src/test/UDPC_NetworkTest.c` is used for testing UDPConnection
and is also an example of using the library in a C program.

## Debug Builds

NetworkTest only builds when CMAKE\_BUILD\_TYPE is Debug (default).

UnitTest only builds in Debug mode and if GTest (a unit testing framework) is
available.

# Links
https://github.com/Stephen-Seo/UDPConnection  
https://git.seodisparate.com/stephenseo/UDPConnection
