// vim: foldmethod=marker
#ifndef UDPC_DEFINES_HPP
#define UDPC_DEFINES_HPP

#define UDPC_CONTEXT_IDENTIFIER 0x902F4DB3
#define UDPC_SENT_PKTS_MAX_SIZE 33
#define UDPC_QUEUED_PKTS_MAX_SIZE 64
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
#include <random>
#include <memory>
#include <thread>
#include <mutex>
#include <iostream>
#include <shared_mutex>

#include "TSLQueue.hpp"
#include "UDPC.h"

#ifdef UDPC_LIBSODIUM_ENABLED
# include <sodium.h>
#endif

#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
# include <ws2tcpip.h>
#elif UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM_LINUX
# include <netdb.h>
#endif

#define UDPC_MIN_HEADER_SIZE 20
#define UDPC_CON_HEADER_SIZE (UDPC_MIN_HEADER_SIZE+4)
#define UDPC_CCL_HEADER_SIZE (UDPC_MIN_HEADER_SIZE+4+crypto_sign_PUBLICKEYBYTES+12)
#define UDPC_CSR_HEADER_SIZE (UDPC_MIN_HEADER_SIZE+4+crypto_sign_PUBLICKEYBYTES+crypto_sign_BYTES)
#define UDPC_LSFULL_HEADER_SIZE (UDPC_MIN_HEADER_SIZE+1+crypto_sign_BYTES)
#define UDPC_NSFULL_HEADER_SIZE (UDPC_MIN_HEADER_SIZE+1)

#define UDPC_UPDATE_MS_MIN 4
#define UDPC_UPDATE_MS_MAX 333
#define UDPC_UPDATE_MS_DEFAULT 8

namespace UDPC {

constexpr auto ONE_SECOND = std::chrono::seconds(1);
constexpr auto TEN_SECONDS = std::chrono::seconds(10);
constexpr auto THIRTY_SECONDS = std::chrono::seconds(30);

constexpr auto INIT_PKT_INTERVAL_DT = std::chrono::seconds(5);
constexpr auto HEARTBEAT_PKT_INTERVAL_DT = std::chrono::milliseconds(150);
constexpr auto PACKET_TIMEOUT_TIME = ONE_SECOND;
constexpr auto GOOD_RTT_LIMIT = std::chrono::milliseconds(250);
constexpr auto CONNECTION_TIMEOUT = TEN_SECONDS;
constexpr auto GOOD_MODE_SEND_RATE = std::chrono::microseconds(33333);
constexpr auto BAD_MODE_SEND_RATE = std::chrono::milliseconds(100);

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
    std::size_t operator()(const UDPC_IPV6_ADDR_TYPE& addr) const;
};

struct PKContainer {
    PKContainer();
    PKContainer(const unsigned char *pk);

    unsigned char pk[crypto_sign_PUBLICKEYBYTES];

    std::size_t operator()(const PKContainer& container) const;
    bool operator==(const PKContainer& other) const;
};

struct ConnectionData {
    ConnectionData(bool isUsingLibsodium);
    ConnectionData(
        bool isServer,
        Context *ctx,
        UDPC_IPV6_ADDR_TYPE addr,
        uint32_t scope_id,
        uint16_t port,
        bool isUsingLibsodium,
        unsigned char *sk,
        unsigned char *pk);
    ~ConnectionData();

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
     * 6 - using libsodium for header verification
     */
    std::bitset<8> flags;
    uint32_t id;
    uint32_t lseq;
    uint32_t rseq;
    uint32_t ack;
    std::chrono::steady_clock::duration timer;
    std::chrono::steady_clock::duration toggleT;
    std::chrono::steady_clock::duration toggleTimer;
    std::chrono::steady_clock::duration toggledTimer;
    UDPC_IPV6_ADDR_TYPE addr; // in network order
    uint32_t scope_id;
    uint16_t port; // in native order
    std::deque<UDPC_PacketInfo> sentPkts;
    std::deque<UDPC_PacketInfo> sendPkts;
    std::deque<UDPC_PacketInfo> priorityPkts;
    // pkt id to pkt shared_ptr
    std::unordered_map<uint32_t, SentPktInfo::Ptr> sentInfoMap;
    std::chrono::steady_clock::time_point received;
    std::chrono::steady_clock::time_point sent;
    std::chrono::steady_clock::duration rtt;
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char peer_pk[crypto_sign_PUBLICKEYBYTES];
    // client - 4 byte string size, rest is string
    // server - detached signature of size crypto_sign_BYTES
    std::unique_ptr<char[]> verifyMessage;
}; // struct ConnectionData

struct Context {
public:
    Context(bool isThreaded);
    ~Context();

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
        switch(type) {
        case UDPC_LoggingType::UDPC_ERROR:
            std::cerr << "ERROR: ";
            break;
        case UDPC_LoggingType::UDPC_WARNING:
            std::cerr << "WARN: ";
            break;
        case UDPC_LoggingType::UDPC_VERBOSE:
            std::cerr << "VERB: ";
            break;
        case UDPC_LoggingType::UDPC_INFO:
            std::cerr << "INFO: ";
            break;
        case UDPC_LoggingType::UDPC_DEBUG:
            std::cerr << "DEBUG: ";
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
        std::cerr << value;
        log_impl_next(type, args...);
    } // }}}

    template<typename... Targs>
    void log_impl_next(UDPC_LoggingType type, UDPC_IPV6_ADDR_TYPE addr, Targs... args) { // {{{
        std::cerr << UDPC_atostr((UDPC_HContext)this, addr);
        log_impl_next(type, args...);
    } // }}}

public:
    void update_impl();

    uint_fast32_t _contextIdentifier;

    char recvBuf[UDPC_PACKET_MAX_SIZE];
    /*
     * 0 - is destucting
     * 1 - is client
     * 2 - libsodium enabled
     */
    std::bitset<8> flags;
    std::atomic_bool isAcceptNewConnections;
    std::atomic_bool isReceivingEvents;
    std::atomic_bool isAutoUpdating;
    std::atomic_uint32_t protocolID;
    std::atomic_uint_fast8_t loggingType;
    // See UDPC_AuthPolicy enum in UDPC.h for possible values
    std::atomic_uint_fast8_t authPolicy;
    char atostrBuf[UDPC_ATOSTR_SIZE];

    UDPC_SOCKETTYPE socketHandle;
    UDPC_IPV6_SOCKADDR_TYPE socketInfo;

    std::chrono::steady_clock::time_point lastUpdated;
    // ipv6 address and port (as UDPC_ConnectionId) to ConnectionData
    std::unordered_map<UDPC_ConnectionId, ConnectionData, ConnectionIdHasher> conMap;
    // ipv6 address to all connected UDPC_ConnectionId
    std::unordered_map<UDPC_IPV6_ADDR_TYPE, std::unordered_set<UDPC_ConnectionId, ConnectionIdHasher>, IPV6_Hasher> addrConMap;
    // id to ipv6 address and port (as UDPC_ConnectionId)
    std::unordered_map<uint32_t, UDPC_ConnectionId> idMap;
    std::unordered_set<UDPC_ConnectionId, ConnectionIdHasher> deletionMap;
    std::unordered_set<PKContainer, PKContainer> peerPKWhitelist;
    std::deque<UDPC_PacketInfo> receivedPkts;
    std::mutex receivedPktsMutex;
    TSLQueue<UDPC_PacketInfo> cSendPkts;
    // handled internally
    std::deque<UDPC_Event> internalEvents;
    std::mutex internalEventsMutex;
    // handled via interface, if isReceivingEvents is true
    std::deque<UDPC_Event> externalEvents;
    std::mutex externalEventsMutex;

    std::default_random_engine rng_engine;

    std::thread thread;
    std::atomic_bool threadRunning;
    std::mutex conMapMutex;
    std::shared_mutex peerPKWhitelistMutex;

    std::chrono::milliseconds threadedSleepTime;
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    std::atomic_bool keysSet;

    std::mutex atostrBufIndexMutex;
    std::uint32_t atostrBufIndex;

    std::mutex setThreadedUpdateMutex;
    std::atomic_uint32_t enableDisableFuncRunningCount;

    std::chrono::milliseconds heartbeatDuration;
    std::shared_mutex heartbeatMutex;

}; // struct Context

Context *verifyContext(UDPC_HContext ctx);

bool isBigEndian();

void be64(char *integer);
void be64_copy(char *out, const char *in);

/*
 * flags:
 *   0x1 - connect
 *   0x2 - ping
 *   0x4 - no_rec_chk
 *   0x8 - resending
 */
void preparePacket(char *data, uint32_t protocolID, uint32_t conID,
                   uint32_t rseq, uint32_t ack, uint32_t *seqID, int flags);

uint32_t generateConnectionID(Context &ctx);

float durationToFSec(const std::chrono::steady_clock::duration& duration);

uint16_t durationToMS(const std::chrono::steady_clock::duration& duration);

float timePointsToFSec(
    const std::chrono::steady_clock::time_point& older,
    const std::chrono::steady_clock::time_point& newer);

UDPC_PacketInfo get_empty_pinfo();

void threadedUpdate(Context *ctx);

} // namespace UDPC

bool operator ==(const UDPC_ConnectionId& a, const UDPC_ConnectionId& b);

bool operator ==(const UDPC_IPV6_ADDR_TYPE& a, const UDPC_IPV6_ADDR_TYPE& b);

#endif
