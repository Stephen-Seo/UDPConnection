// vim: foldmethod=marker
#ifndef UDPC_DEFINES_HPP
#define UDPC_DEFINES_HPP

#define UDPC_CONTEXT_IDENTIFIER 0x902F4DB3
#define UDPC_SENT_PKTS_MAX_SIZE 33
#define UDPC_QUEUED_PKTS_MAX_SIZE 32
#define UDPC_RECEIVED_PKTS_MAX_SIZE 64

#define UDPC_ID_CONNECT 0x80000000
#define UDPC_ID_PING 0x40000000
#define UDPC_ID_NO_REC_CHK 0x20000000
#define UDPC_ID_RESENDING 0x10000000

#define UDPC_ATOSTR_BUFCOUNT 32
#define UDPC_ATOSTR_BUFSIZE 40
#define UDPC_ATOSTR_SIZE (UDPC_ATOSTR_BUFCOUNT * UDPC_ATOSTR_BUFSIZE)

#define UDPC_CHECK_LOG(ctx, type, ...) if(ctx->willLog(type)){ctx->log(type, __VA_ARGS__);}

#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <random>
#include <memory>
#include <thread>
#include <mutex>
#include <iostream>

#include "TSQueue.hpp"
#include "UDPConnection.h"

#include <sodium.h>

#define UDPC_MIN_HEADER_SIZE 20
#define UDPC_CON_HEADER_SIZE (UDPC_MIN_HEADER_SIZE + crypto_sign_PUBLICKEYBYTES)
#define UDPC_FULL_HEADER_SIZE (UDPC_MIN_HEADER_SIZE + crypto_sign_BYTES)

namespace UDPC {

static const auto ONE_SECOND = std::chrono::seconds(1);
static const auto TEN_SECONDS = std::chrono::seconds(10);
static const auto THIRTY_SECONDS = std::chrono::seconds(30);

static const auto INIT_PKT_INTERVAL_DT = std::chrono::seconds(5);
static const auto HEARTBEAT_PKT_INTERVAL_DT = std::chrono::milliseconds(150);
static const auto PACKET_TIMEOUT_TIME = ONE_SECOND;
static const auto GOOD_RTT_LIMIT = std::chrono::milliseconds(250);
static const auto CONNECTION_TIMEOUT = TEN_SECONDS;
static const auto GOOD_MODE_SEND_RATE = std::chrono::microseconds(33333);
static const auto BAD_MODE_SEND_RATE = std::chrono::milliseconds(100);

// forward declaration
struct Context;

struct SentPktInfo {
    typedef std::shared_ptr<SentPktInfo> Ptr;

    SentPktInfo();

    uint32_t id;
    std::chrono::steady_clock::time_point sentTime;
};

struct ConnectionIdHasher {
    std::size_t operator()(const UDPC_ConnectionId& key) const;
};

struct IPV6_Hasher {
    std::size_t operator()(const struct in6_addr& addr) const;
};

struct ConnectionData {
    ConnectionData();
    ConnectionData(bool isServer, Context *ctx, struct in6_addr addr, uint32_t scope_id, uint16_t port);

    // copy
    ConnectionData(const ConnectionData& other) = delete;
    ConnectionData& operator=(const ConnectionData& other) = delete;

    // move
    ConnectionData(ConnectionData&& other) = default;
    ConnectionData& operator=(ConnectionData&& other) = default;

    void cleanupSentPkts();

    /*
     * 0 - trigger send
     * 1 - is good mode
     * 2 - is good rtt
     * 3 - initiating connection
     * 4 - is id set
     * 5 - error initializing keys for public key encryption
     */
    std::bitset<32> flags;
    uint32_t id;
    uint32_t lseq;
    uint32_t rseq;
    uint32_t ack;
    std::chrono::steady_clock::duration timer;
    std::chrono::steady_clock::duration toggleT;
    std::chrono::steady_clock::duration toggleTimer;
    std::chrono::steady_clock::duration toggledTimer;
    struct in6_addr addr; // in network order
    uint32_t scope_id;
    uint16_t port; // in native order
    std::deque<UDPC_PacketInfo> sentPkts;
    TSQueue<UDPC_PacketInfo> sendPkts;
    TSQueue<UDPC_PacketInfo> priorityPkts;
    // pkt id to pkt shared_ptr
    std::unordered_map<uint32_t, SentPktInfo::Ptr> sentInfoMap;
    std::chrono::steady_clock::time_point received;
    std::chrono::steady_clock::time_point sent;
    std::chrono::steady_clock::duration rtt;
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char peer_pk[crypto_sign_PUBLICKEYBYTES];
}; // struct ConnectionData

struct Context {
public:
    Context(bool isThreaded);

    bool willLog(UDPC_LoggingType);

    void log(UDPC_LoggingType) {}

    template<typename... Targs>
    void log(UDPC_LoggingType type, Targs... args) {
        log_impl(type, args...);
    }

private:
    void log_impl(UDPC_LoggingType) {}

    template<typename... Targs>
    void log_impl(UDPC_LoggingType type, Targs... args) { // {{{
        switch(loggingType.load()) {
        case UDPC_LoggingType::UDPC_SILENT:
            return;
        case UDPC_LoggingType::UDPC_ERROR:
            if(type == UDPC_LoggingType::UDPC_ERROR) {
                std::cerr << "ERROR: ";
            } else {
                return;
            }
            break;
        case UDPC_LoggingType::UDPC_WARNING:
            switch(type) {
            case UDPC_LoggingType::UDPC_ERROR:
                std::cerr << "ERROR: ";
                break;
            case UDPC_LoggingType::UDPC_WARNING:
                std::cerr << "WARNING: ";
                break;
            default:
                return;
            }
            break;
        case UDPC_LoggingType::UDPC_VERBOSE:
            switch(type) {
            case UDPC_LoggingType::UDPC_ERROR:
                std::cerr << "ERROR: ";
                break;
            case UDPC_LoggingType::UDPC_WARNING:
                std::cerr << "WARNING: ";
                break;
            case UDPC_LoggingType::UDPC_VERBOSE:
                std::cerr << "VERBOSE: ";
                break;
            default:
                return;
            }
            break;
        case UDPC_LoggingType::UDPC_INFO:
            switch(type) {
            case UDPC_LoggingType::UDPC_ERROR:
                std::cerr << "ERROR: ";
                break;
            case UDPC_LoggingType::UDPC_WARNING:
                std::cerr << "WARNING: ";
                break;
            case UDPC_LoggingType::UDPC_VERBOSE:
                std::cerr << "VERBOSE: ";
                break;
            case UDPC_LoggingType::UDPC_INFO:
                std::cerr << "INFO: ";
                break;
            default:
                return;
            }
            break;
        default:
            return;
        }

        log_impl_next(type, args...);
    } // }}}

    void log_impl_next(UDPC_LoggingType) {
        std::cerr << '\n';
    }

    template<typename T, typename... Targs>
    void log_impl_next(UDPC_LoggingType type, T value, Targs... args) { // {{{
        switch(loggingType.load()) {
        case UDPC_LoggingType::UDPC_SILENT:
            return;
        case UDPC_LoggingType::UDPC_ERROR:
            if(type == UDPC_LoggingType::UDPC_ERROR) {
                std::cerr << value;
            }
            break;
        case UDPC_LoggingType::UDPC_WARNING:
            if(type == UDPC_LoggingType::UDPC_ERROR || type == UDPC_LoggingType::UDPC_WARNING) {
                std::cerr << value;
            }
            break;
        case UDPC_LoggingType::UDPC_VERBOSE:
            if(type == UDPC_LoggingType::UDPC_ERROR || type == UDPC_LoggingType::UDPC_WARNING
                    || type == UDPC_LoggingType::UDPC_VERBOSE) {
                std::cerr << value;
            }
            break;
        case UDPC_LoggingType::UDPC_INFO:
            if(type == UDPC_LoggingType::UDPC_ERROR || type == UDPC_LoggingType::UDPC_WARNING
                    || type == UDPC_LoggingType::UDPC_VERBOSE
                    || type == UDPC_LoggingType::UDPC_INFO) {
                std::cerr << value;
            }
            break;
        }
        log_impl_next(type, args...);
    } // }}}

public:
    void update_impl();

    uint_fast32_t _contextIdentifier;

    char recvBuf[UDPC_PACKET_MAX_SIZE];
    /*
     * 0 - is threaded
     * 1 - is client
     */
    std::bitset<32> flags;
    std::atomic_bool isAcceptNewConnections;
    std::atomic_uint32_t protocolID;
    std::atomic_uint_fast8_t loggingType;
    std::atomic_uint32_t atostrBufIndex;
    char atostrBuf[UDPC_ATOSTR_SIZE];

    UDPC_SOCKETTYPE socketHandle;
    struct sockaddr_in6 socketInfo;

    std::chrono::steady_clock::time_point lastUpdated;
    // ipv6 address and port (as UDPC_ConnectionId) to ConnectionData
    std::unordered_map<UDPC_ConnectionId, ConnectionData, ConnectionIdHasher> conMap;
    // ipv6 address to all connected UDPC_ConnectionId
    std::unordered_map<struct in6_addr, std::unordered_set<UDPC_ConnectionId, ConnectionIdHasher>, IPV6_Hasher> addrConMap;
    // id to ipv6 address and port (as UDPC_ConnectionId)
    std::unordered_map<uint32_t, UDPC_ConnectionId> idMap;
    TSQueue<UDPC_PacketInfo> receivedPkts;

    std::default_random_engine rng_engine;

    std::thread thread;
    std::atomic_bool threadRunning;
    std::mutex mutex;

}; // struct Context

Context *verifyContext(UDPC_HContext ctx);

bool isBigEndian();

/*
 * flags:
 *   - 0x1 - connect
 *   - 0x2 - ping
 *   - 0x4 - no_rec_chk
 *   - 0x8 - resending
 */
void preparePacket(char *data, uint32_t protocolID, uint32_t conID,
                   uint32_t rseq, uint32_t ack, uint32_t *seqID, int flags,
                   const unsigned char *pk, const unsigned char *sk);

uint32_t generateConnectionID(Context &ctx);

float durationToFSec(const std::chrono::steady_clock::duration& duration);

float timePointsToFSec(
    const std::chrono::steady_clock::time_point& older,
    const std::chrono::steady_clock::time_point& newer);

UDPC_PacketInfo get_empty_pinfo();

void threadedUpdate(Context *ctx);

} // namespace UDPC

bool operator ==(const UDPC_ConnectionId& a, const UDPC_ConnectionId& b);

bool operator ==(const struct in6_addr& a, const struct in6_addr& b);

#endif
