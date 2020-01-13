# UDPConnection

Provides a network connection over UDP, with verification of packet support via
libsodium (optional). Implemented in C++, but is available via a C api, which
should facilitate creating bindings for other programming languages if needed.

This library is still a work in progress, so api breaking changes may happen
in the future.

## Compiling

    mkdir buildRelease
    cd buildRelease
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=True ..
    make
    make DESTDIR=install_destination install

## Usage

The program in `src/test/UDPC_NetworkTest.c` is used for testing UDPConnection
and is also an example of using the library in a C program.
