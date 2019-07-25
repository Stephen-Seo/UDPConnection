#ifndef UDPC_DEFINES_HPP
#define UDPC_DEFINES_HPP

#define UDPC_CONTEXT_IDENTIFIER 0x902F4DB3
#define UDPC_TIMEOUT_SECONDS 10.0f
#define UDPC_GOOD_MODE_SEND_INTERVAL (1.0f / 30.0f)
#define UDPC_BAD_MODE_SEND_INTERVAL (1.0f / 10.0f)
#define UDPC_SENT_PKTS_MAX_SIZE 33

#define UDPC_ID_CONNECT 0x80000000
#define UDPC_ID_PING 0x40000000
#define UDPC_ID_NO_REC_CHK 0x20000000
#define UDPC_ID_RESENDING 0x10000000

#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <deque>
#include <unordered_map>

#include "TSQueue.hpp"
#include "UDPConnection.h"

namespace UDPC {

static uint32_t LOCAL_ADDR = 0;
static const auto INIT_PKT_INTERVAL_DT = std::chrono::seconds(5);
static const auto HEARTBEAT_PKT_INTERVAL_DT = std::chrono::milliseconds(150);

struct ConnectionData {
    /*
     * 0 - trigger send
     * 1 - is good mode
     * 2 - is good rtt
     * 3 - initiating connection
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
    std::deque<UDPC_PacketInfo> sentPkts;
    TSQueue<UDPC_PacketInfo> sendPkts;
    TSQueue<UDPC_PacketInfo> priorityPkts;
    std::chrono::steady_clock::time_point received;
    std::chrono::steady_clock::time_point sent;
    float rtt;
}; // struct ConnectionData

struct Context {
    Context(bool isThreaded);

    uint_fast32_t _contextIdentifier;
    /*
     * 0 - is threaded
     * 1 - is client
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
    std::unordered_map<uint32_t, uint32_t> idMap;

}; // struct Context

Context *verifyContext(void *ctx);

bool isBigEndian();

/*
 * flags:
 *   - 0x1 - connect
 *   - 0x2 - ping
 *   - 0x4 - no_rec_chk
 *   - 0x8 - resending
 */
void preparePacket(char *data, uint32_t protocolID, uint32_t conID,
                   uint32_t rseq, uint32_t ack, uint32_t *seqID, int flags);

} // namespace UDPC

#endif
