#ifndef UDPC_DEFINES_HPP
#define UDPC_DEFINES_HPP

#define UDPC_CONTEXT_IDENTIFIER 0x902F4DB3

#include <atomic>
#include <bitset>
#include <cstdint>

#include "UDPConnection.h"

namespace UDPC {

struct Context {
    Context(bool isThreaded);

    uint_fast32_t _contextIdentifier;
    /*
     * 0 - isThreaded
     */
    std::bitset<32> flags;
    std::atomic_bool isAcceptNewConnections;
    std::atomic_uint32_t protocolID;
    std::atomic_uint_fast8_t loggingType;
    char atostrBuf[16];
}; // struct Context

bool VerifyContext(void *ctx);

bool isBigEndian();

} // namespace UDPC

#endif
