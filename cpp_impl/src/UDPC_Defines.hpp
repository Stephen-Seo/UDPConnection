// vim: foldmethod=marker
#ifndef UDPC_DEFINES_HPP
#define UDPC_DEFINES_HPP

#define UDPC_CONTEXT_IDENTIFIER 0x902F4DB3
#define UDPC_SENT_PKTS_MAX_SIZE 33
#define UDPC_QUEUED_PKTS_MAX_SIZE 32
#define UDPC_RECEIVED_PKTS_MAX_SIZE 50

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
#include <unordered_set>
#include <queue>
#include <random>
#include <memory>
#include <thread>
#include <iostream>

#include "TSQueue.hpp"
#include "UDPConnection.h"

namespace UDPC {

static const auto ONE_SECOND = std::chrono::seconds(1);
static const auto TEN_SECONDS = std::chrono::seconds(10);
static const auto THIRTY_SECONDS = std::chrono::seconds(30);

static uint32_t LOCAL_ADDR = 0;
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

struct ConnectionData {
    ConnectionData();
    ConnectionData(bool isServer, Context *ctx);

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
    uint32_t addr; // in network order
    uint16_t port; // in native order
    std::deque<UDPC_PacketInfo> sentPkts;
    TSQueue<UDPC_PacketInfo> sendPkts;
    TSQueue<UDPC_PacketInfo> priorityPkts;
    TSQueue<UDPC_PacketInfo> receivedPkts;
    // pkt id to pkt shared_ptr
    std::unordered_map<uint32_t, SentPktInfo::Ptr> sentInfoMap;
    std::chrono::steady_clock::time_point received;
    std::chrono::steady_clock::time_point sent;
    std::chrono::steady_clock::duration rtt;
}; // struct ConnectionData

struct Context {
public:
    Context(bool isThreaded);

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
        case UDPC_LoggingType::SILENT:
            return;
        case UDPC_LoggingType::ERROR:
            if(type == UDPC_LoggingType::ERROR) {
                std::cerr << "ERROR: ";
            } else {
                return;
            }
            break;
        case UDPC_LoggingType::WARNING:
            switch(type) {
            case UDPC_LoggingType::ERROR:
                std::cerr << "ERROR: ";
                break;
            case UDPC_LoggingType::WARNING:
                std::cerr << "WARNING: ";
                break;
            default:
                return;
            }
            break;
        case UDPC_LoggingType::VERBOSE:
            switch(type) {
            case UDPC_LoggingType::ERROR:
                std::cerr << "ERROR: ";
                break;
            case UDPC_LoggingType::WARNING:
                std::cerr << "WARNING: ";
                break;
            case UDPC_LoggingType::VERBOSE:
                std::cerr << "VERBOSE: ";
                break;
            default:
                return;
            }
            break;
        case UDPC_LoggingType::INFO:
            switch(type) {
            case UDPC_LoggingType::ERROR:
                std::cerr << "ERROR: ";
                break;
            case UDPC_LoggingType::WARNING:
                std::cerr << "WARNING: ";
                break;
            case UDPC_LoggingType::VERBOSE:
                std::cerr << "VERBOSE: ";
                break;
            case UDPC_LoggingType::INFO:
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
        case UDPC_LoggingType::SILENT:
            return;
        case UDPC_LoggingType::ERROR:
            if(type == UDPC_LoggingType::ERROR) {
                std::cerr << value;
            }
            break;
        case UDPC_LoggingType::WARNING:
            if(type == UDPC_LoggingType::ERROR || type == UDPC_LoggingType::WARNING) {
                std::cerr << value;
            }
            break;
        case UDPC_LoggingType::VERBOSE:
            if(type == UDPC_LoggingType::ERROR || type == UDPC_LoggingType::WARNING
                    || type == UDPC_LoggingType::VERBOSE) {
                std::cerr << value;
            }
            break;
        case UDPC_LoggingType::INFO:
            if(type == UDPC_LoggingType::ERROR || type == UDPC_LoggingType::WARNING
                    || type == UDPC_LoggingType::VERBOSE
                    || type == UDPC_LoggingType::INFO) {
                std::cerr << value;
            }
            break;
        }
        log_impl_next(type, args...);
    } // }}}

public:
    uint_fast32_t _contextIdentifier;

    char recvBuf[UDPC_PACKET_MAX_SIZE];
    /*
     * 0 - is threaded
     * 1 - is client
     * 2 - is accepting new connections
     */
    std::bitset<32> flags;
    std::atomic_bool isAcceptNewConnections;
    std::atomic_uint32_t protocolID;
    std::atomic_uint_fast8_t loggingType;
    char atostrBuf[16];

    int socketHandle;
    struct sockaddr_in socketInfo;

    std::chrono::steady_clock::time_point lastUpdated;
    // ipv4 address and port (as UDPC_ConnectionId) to ConnectionData
    std::unordered_map<UDPC_ConnectionId, ConnectionData, ConnectionIdHasher> conMap;
    // ipv4 address to all connected UDPC_ConnectionId
    std::unordered_map<uint32_t, std::unordered_set<UDPC_ConnectionId, ConnectionIdHasher> > addrConMap;
    // id to ipv4 address and port (as UDPC_ConnectionId)
    std::unordered_map<uint32_t, UDPC_ConnectionId> idMap;

    std::default_random_engine rng_engine;

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
                   uint32_t rseq, uint32_t ack, uint32_t *seqID, int flags);

uint32_t generateConnectionID(Context &ctx);

float durationToFSec(const std::chrono::steady_clock::duration& duration);

float timePointsToFSec(
    const std::chrono::steady_clock::time_point& older,
    const std::chrono::steady_clock::time_point& newer);

} // namespace UDPC

bool operator ==(const UDPC_ConnectionId& a, const UDPC_ConnectionId& b);

#endif
