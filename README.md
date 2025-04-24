# UDPConnection

Conan package is currently unavailable. However, building this with CMake should
be simple.

Provides a network connection over UDP, with verification of packet support via
libsodium (optional). Implemented in C++ (up to C++11 standard), but is
available via a C api, which should facilitate creating bindings for other
programming languages if needed.

~~This library is still a work in progress, so api breaking changes may happen
in the future.~~  
This library is usable. Try testing it out using the `NetworkTest` binary which
is built when Debug builds are enabled (the default) with CMake.

    $ ./NetworkTest
    [-c | -s] - client or server (default server)
    -ll <addr> - listen addr
    -lp <port> - listen port
    -cl <addr> - connection addr (client only)
    -clh <hostname> - connection hostname (client only)
    -cp <port> - connection port (client only)
    -t <tick_count>
    -n - do not add payload to packets
    -l (silent|error|warning|info|verbose|debug) - log level, default debug
    -e - enable receiving events
    -ls - enable libsodium
    -ck <pubkey_file> - add pubkey to whitelist
    -sk <pubkey> <seckey> - start with pub/sec key pair
    -p <"fallback" or "strict"> - set auth policy
    --hostname <hostname> - dont run test, just lookup hostname

A typical test can be done with the following parameters:

Server:

    ./NetworkTest -s -ll ::1 -lp 9000 -t 50 -e

Client:

    ./NetworkTest -c -ll ::1 -lp 9001 -cl ::1 -cp 9000 -t 40 -e

`NetworkTest` gracefully shuts down on SIGINT (Ctrl-C).

The source of `NetworkTest` can be found in `src/test/UDPC_NetworkTest.c`.

## Documentation

[See the gh-pages generated Doxygen documentation here.](https://stephen-seo.github.io/UDPConnection/)

[Alternatively, see the generated Doxygen documentation on my website.](https://seodisparate.com/udpc_docs)

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

UnitTest only builds in Debug mode.

# Links
https://github.com/Stephen-Seo/UDPConnection  

# Other Notes

Inspired by [a series of blog posts about networking for games.](https://gafferongames.com/categories/game-networking/)

## C++11 vs. C++17

Version 1.2 of UDPConnection supports up to C++11 standard, but future versions
target the C++17 standard. If this is a problem for your use case, then you can
safely use version 1.2 of UDPConnection for now. If there are major changes or
fixes since, it will be noted here.

## Conan

There used to be a self-hosted conan repository for UDPConnection. It is
currently unavailable.

The `conan` branch contains the necessary changes to publish this library as a
conan package. Expect the `conan` branch to merge `master` in the future.

## Removal of Setting Heartbeat Interval Function

https://github.com/Stephen-Seo/UDPConnection/issues/2

(The following text is a reproduction of the text in the linked issue in case
the link goes down.)

    The original intent of the reverted function UDPC_set_heartbeat_millis(...)
    was to allow UDPC to accommodate the use case of where packets are sent much
    less frequently than the "good-mode"/"bad-mode" rates (30pkts-per-second and
    10pkts-per-second). However, increasing the heartbeat packet rate and also
    increasing the timed-out-packet time causes issues with the current
    implementation.
    
    The default packet-time-out time is 1 second. Every packet includes a 32-bit
    ack that keeps track of the peer's packet's received status. This means that
    at the fastest rate of 30 packets a second, these 32 flags can still track
    if a packet has timed out, and a peer will resend a timed out packet
    accordingly.
    
    The previously introduced functionality of increasing the
    heartbeat-interval-time (and packet-timeout-time based on the
    heartbeat-interval-time) breaks this setup in the case where a burst of
    fastest-rate packets are sent followed by only heartbeat packets, causing
    possible loss of tracked packets and preventing them from being re-sent (or
    being re-sent too aggressively).
    
    As mentioned in 79f43a4 , some changes are required to support high-latency
    use cases. One possible use is to impose a separate time-out rate and
    send-rate for high-latency use cases. But it would be more ideal for UDPC to
    do the heavy lifting and handle both situations seamlessly.

Long story short, for high-latency, just use TCP/Websockets etc. For
low-latency, you may use this library, but the connection must be decent.
