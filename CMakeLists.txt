cmake_minimum_required(VERSION 3.7)
project(UDPConnection)

set(UDPConnection_SOURCES
    src/UDPConnection.c
)

add_library(UDPConnection ${UDPConnection_SOURCES})


