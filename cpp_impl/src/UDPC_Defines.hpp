#ifndef UDPC_DEFINES_HPP
#define UDPC_DEFINES_HPP

#define UDPC_CONTEXT_IDENTIFIER 0x902F4DB3
#define UDPC_TIMEOUT_SECONDS 10.0f

#include <atomic>
#include <bitset>
#include <cstdint>
#include <deque>
#include <chrono>
#include <unordered_map>

#include "UDPConnection.h"
#include "TSQueue.hpp"

namespace UDPC {

struct ConnectionData {
    /*
     * 0 - trigger send
     * 1 - is good mode
     * 2 - is good rtt
     * 3 - initiating connection to server
     * 4 - is id set
     */
    std::bitset<32> flags;
    uint32_t id;
    uint32_t lseq;
    uint32_t rseq;
    uint32_t ack;
    float timer;
    float toggleT;
    float toggleTimer;
    float toggledTimer;
    uint32_t addr; // in network order
    uint16_t port;
    std::deque<PacketInfo> sentPkts;
    TSQueue<PacketInfo> sendPkts;
    TSQueue<PacketInfo> priorityPkts;
    std::chrono::steady_clock::time_point received;
    std::chrono::steady_clock::time_point sent;
    float rtt;
}; // struct ConnectionData

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

    int socketHandle;
    struct sockaddr_in socketInfo;

    std::chrono::steady_clock::time_point lastUpdated;
    std::unordered_map<uint32_t, ConnectionData> conMap;
}; // struct Context

Context* verifyContext(void *ctx);

bool isBigEndian();

} // namespace UDPC

#endif
