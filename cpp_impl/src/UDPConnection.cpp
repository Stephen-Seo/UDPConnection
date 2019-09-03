#include "UDPC_Defines.hpp"
#include "UDPConnection.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <optional>
#include <vector>
#include <functional>
#include <type_traits>

UDPC::SentPktInfo::SentPktInfo() :
id(0),
sentTime(std::chrono::steady_clock::now())
{}

std::size_t UDPC::ConnectionIdHasher::operator()(const UDPC_ConnectionId& key) const {
    return std::hash<uint32_t>()(key.addr) ^ (std::hash<uint16_t>()(key.port) << 1);
}

bool operator ==(const UDPC_ConnectionId& a, const UDPC_ConnectionId& b) {
    return a.addr == b.addr && a.port == b.port;
}

UDPC::ConnectionData::ConnectionData() :
flags(),
id(0),
lseq(0),
rseq(0),
ack(0xFFFFFFFF),
timer(std::chrono::steady_clock::duration::zero()),
toggleT(UDPC::THIRTY_SECONDS),
toggleTimer(std::chrono::steady_clock::duration::zero()),
toggledTimer(std::chrono::steady_clock::duration::zero()),
sentPkts(),
sendPkts(UDPC_QUEUED_PKTS_MAX_SIZE),
priorityPkts(UDPC_QUEUED_PKTS_MAX_SIZE),
receivedPkts(UDPC_RECEIVED_PKTS_MAX_SIZE),
received(std::chrono::steady_clock::now()),
sent(std::chrono::steady_clock::now()),
rtt(std::chrono::steady_clock::duration::zero())
{
    flags.set(0);
}

UDPC::ConnectionData::ConnectionData(bool isServer, Context *ctx) :
UDPC::ConnectionData::ConnectionData()
{
    flags.set(3);
    if(isServer) {
        id = UDPC::generateConnectionID(*ctx);
        flags.set(4);
    } else {
        lseq = 1;
    }
}

void UDPC::ConnectionData::cleanupSentPkts() {
    uint32_t id;
    while(sentPkts.size() > UDPC_SENT_PKTS_MAX_SIZE) {
        id = *((uint32_t*)(sentPkts.front().data + 8));
        auto iter = sentInfoMap.find(id);
        assert(iter != sentInfoMap.end()
                && "Sent packet must have correspoding entry in sentInfoMap");
        sentInfoMap.erase(iter);
        sentPkts.pop_front();
    }
}

UDPC::Context::Context(bool isThreaded) :
_contextIdentifier(UDPC_CONTEXT_IDENTIFIER),
flags(),
isAcceptNewConnections(true),
protocolID(UDPC_DEFAULT_PROTOCOL_ID),
#ifndef NDEBUG
loggingType(INFO),
#else
loggingType(WARNING),
#endif
rng_engine()
{
    if(isThreaded) {
        flags.set(0);
    } else {
        flags.reset(0);
    }

    if(UDPC::LOCAL_ADDR == 0) {
        if(UDPC::isBigEndian()) {
            UDPC::LOCAL_ADDR = 0x7F000001;
        } else {
            UDPC::LOCAL_ADDR = 0x0100007F;
        }
    }

    rng_engine.seed(std::chrono::system_clock::now().time_since_epoch().count());
}

UDPC::Context *UDPC::verifyContext(UDPC_HContext ctx) {
    if(ctx == nullptr) {
        return nullptr;
    }
    UDPC::Context *c = (UDPC::Context *)ctx;
    if(c->_contextIdentifier == UDPC_CONTEXT_IDENTIFIER) {
        return c;
    } else {
        return nullptr;
    }
}

bool UDPC::isBigEndian() {
    static std::optional<bool> isBigEndian = std::nullopt;
    if(isBigEndian) {
        return *isBigEndian;
    }
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    isBigEndian = (bint.c[0] == 1);
    return *isBigEndian;
}

void UDPC::preparePacket(char *data, uint32_t protocolID, uint32_t conID,
                         uint32_t rseq, uint32_t ack, uint32_t *seqID,
                         int flags) {
    uint32_t temp;

    temp = htonl(protocolID);
    std::memcpy(data, &temp, 4);
    temp = htonl(conID | ((flags & 0x1) != 0 ? UDPC_ID_CONNECT : 0) |
                 ((flags & 0x2) != 0 ? UDPC_ID_PING : 0) |
                 ((flags & 0x4) != 0 ? UDPC_ID_NO_REC_CHK : 0) |
                 ((flags & 0x8) != 0 ? UDPC_ID_RESENDING : 0));
    std::memcpy(data + 4, &temp, 4);

    if(seqID) {
        temp = htonl(*seqID);
        ++(*seqID);
    } else {
        temp = 0;
    }
    std::memcpy(data + 8, &temp, 4);

    temp = htonl(rseq);
    std::memcpy(data + 12, &temp, 4);
    temp = htonl(ack);
    std::memcpy(data + 16, &temp, 4);
}

uint32_t UDPC::generateConnectionID(Context &ctx) {
    auto dist = std::uniform_int_distribution<uint32_t>(0, 0xFFFFFFFF);
    uint32_t id = dist(ctx.rng_engine);
    while(ctx.idMap.find(id) != ctx.idMap.end()) {
        id = dist(ctx.rng_engine);
    }
    return id;
}

float UDPC::durationToFSec(const std::chrono::steady_clock::duration& duration) {
    return (float)duration.count()
        * (float)std::decay_t<decltype(duration)>::period::num
        / (float)std::decay_t<decltype(duration)>::period::den;
}

float UDPC::timePointsToFSec(
        const std::chrono::steady_clock::time_point& older,
        const std::chrono::steady_clock::time_point& newer) {
    const auto dt = newer - older;
    return (float)dt.count()
        * (float)decltype(dt)::period::num / (float)decltype(dt)::period::den;
}

UDPC_ConnectionId UDPC_create_id(uint32_t addr, uint16_t port) {
    return UDPC_ConnectionId{addr, port};
}

UDPC_HContext UDPC_init(uint16_t listenPort, uint32_t listenAddr, int isClient) {
    UDPC::Context *ctx = new UDPC::Context(false);
    ctx->flags.set(1, isClient);

    // create socket
    ctx->socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(ctx->socketHandle <= 0) {
        // TODO maybe different way of handling init fail
        delete ctx;
        return nullptr;
    }

    // bind socket
    ctx->socketInfo.sin_family = AF_INET;
    ctx->socketInfo.sin_addr.s_addr =
        (listenAddr == 0 ? INADDR_ANY : listenAddr);
    ctx->socketInfo.sin_port = htons(listenPort);
    if(bind(ctx->socketHandle, (const struct sockaddr *)&ctx->socketInfo,
            sizeof(struct sockaddr_in)) < 0) {
        // TODO maybe different way of handling init fail
        CleanupSocket(ctx->socketHandle);
        delete ctx;
        return nullptr;
    }

    // TODO verify this is necessary to get the listen port
    if(ctx->socketInfo.sin_port == 0) {
        struct sockaddr_in getInfo;
        socklen_t size = sizeof(struct sockaddr_in);
        if(getsockname(ctx->socketHandle, (struct sockaddr *)&getInfo, &size) == 0) {
            ctx->socketInfo.sin_port = getInfo.sin_port;
        }
    }

    // set non-blocking on socket
#if UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
    int nonblocking = 1;
    if(fcntl(ctx->socketHandle, F_SETFL, O_NONBLOCK, nonblocking) == -1) {
#elif UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
    DWORD nonblocking = 1;
    if(ioctlsocket(ctx->socketHandle, FIONBIO, &nonblocking) != 0) {
#else
    {
#endif
        // TODO maybe different way of handling init fail
        CleanupSocket(ctx->socketHandle);
        delete ctx;
        return nullptr;
    }

    return (UDPC_HContext) ctx;
}

UDPC_HContext UDPC_init_threaded_update(uint16_t listenPort, uint32_t listenAddr,
                                int isClient) {
    UDPC::Context *ctx =
        (UDPC::Context *)UDPC_init(listenPort, listenAddr, isClient);
    if(!ctx) {
        return nullptr;
    }
    ctx->flags.set(0);

    return (UDPC_HContext) ctx;
}

void UDPC_destroy(UDPC_HContext ctx) {
    UDPC::Context *UDPC_ctx = UDPC::verifyContext(ctx);
    if(UDPC_ctx) {
        delete UDPC_ctx;
    }
}

void UDPC_update(UDPC_HContext ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c || c->flags.test(0)) {
        // invalid or is threaded, update should not be called
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    c->lastUpdated = now;

    std::chrono::steady_clock::duration temp_dt_fs;
    {
        // check timed out, check good/bad mode with rtt, remove timed out
        std::vector<UDPC_ConnectionId> removed;
        for(auto iter = c->conMap.begin(); iter != c->conMap.end(); ++iter) {
            temp_dt_fs = now - iter->second.received;
            if(temp_dt_fs >= UDPC::CONNECTION_TIMEOUT) {
                removed.push_back(iter->first);
                c->log(
                    UDPC_LoggingType::VERBOSE,
                    "Timed out connection with ",
                    UDPC_atostr(ctx, iter->second.addr),
                    ":",
                    iter->second.port);
                continue;
            }

            // check good/bad mode
            iter->second.toggleTimer += temp_dt_fs;
            iter->second.toggledTimer += temp_dt_fs;
            if(iter->second.flags.test(1) && !iter->second.flags.test(2)) {
                // good mode, bad rtt
                c->log(
                    UDPC_LoggingType::INFO,
                    "Switching to bad mode in connection with ",
                    UDPC_atostr(ctx, iter->second.addr),
                    ":",
                    iter->second.port);
                iter->second.flags.reset(1);
                if(iter->second.toggledTimer <= UDPC::TEN_SECONDS) {
                    iter->second.toggleT *= 2;
                }
                iter->second.toggledTimer = std::chrono::steady_clock::duration::zero();
            } else if(iter->second.flags.test(1)) {
                // good mode, good rtt
                if(iter->second.toggleTimer >= UDPC::TEN_SECONDS) {
                    iter->second.toggleTimer = std::chrono::steady_clock::duration::zero();
                    iter->second.toggleT /= 2;
                    if(iter->second.toggleT < UDPC::ONE_SECOND) {
                        iter->second.toggleT = UDPC::ONE_SECOND;
                    }
                }
            } else if(!iter->second.flags.test(1) &&
                      iter->second.flags.test(2)) {
                // bad mode, good rtt
                if(iter->second.toggledTimer >= iter->second.toggleT) {
                    iter->second.toggleTimer = std::chrono::steady_clock::duration::zero();
                    iter->second.toggledTimer = std::chrono::steady_clock::duration::zero();
                    c->log(
                        UDPC_LoggingType::INFO,
                        "Switching to good mode in connection with ",
                        UDPC_atostr(ctx, iter->second.addr),
                        ":",
                        iter->second.port);
                    iter->second.flags.set(1);
                }
            } else {
                // bad mode, bad rtt
                iter->second.toggledTimer = std::chrono::steady_clock::duration::zero();
            }

            iter->second.timer += temp_dt_fs;
            if(iter->second.timer >= (iter->second.flags.test(1)
                                          ? UDPC::GOOD_MODE_SEND_RATE
                                          : UDPC::BAD_MODE_SEND_RATE)) {
                iter->second.timer -= (iter->second.flags.test(1)
                        ? UDPC::GOOD_MODE_SEND_RATE : UDPC::BAD_MODE_SEND_RATE);
                iter->second.flags.set(0);
            }
        }
        for(auto iter = removed.begin(); iter != removed.end(); ++iter) {
            auto addrConIter = c->addrConMap.find(iter->addr);
            assert(addrConIter != c->addrConMap.end()
                    && "addrConMap must have an entry for a current connection");
            auto addrConSetIter = addrConIter->second.find(*iter);
            assert(addrConSetIter != addrConIter->second.end()
                    && "nested set in addrConMap must have an entry for a current connection");
            addrConIter->second.erase(addrConSetIter);
            if(addrConIter->second.empty()) {
                c->addrConMap.erase(addrConIter);
            }

            auto cIter = c->conMap.find(*iter);
            assert(cIter != c->conMap.end()
                    && "conMap must have the entry set to be removed");

            if(cIter->second.flags.test(4)) {
                c->idMap.erase(cIter->second.id);
            }

            c->conMap.erase(cIter);
        }
    }

    // update send (only if triggerSend flag is set)
    for(auto iter = c->conMap.begin(); iter != c->conMap.end(); ++iter) {
        if(!iter->second.flags.test(0)) {
            continue;
        }
        iter->second.flags.reset(0);

        if(iter->second.flags.test(3)) {
            if(c->flags.test(1)) {
                // is initiating connection to server
                auto initDT = now - iter->second.sent;
                if(initDT < UDPC::INIT_PKT_INTERVAL_DT) {
                    continue;
                }
                iter->second.sent = now;

                std::unique_ptr<char[]> buf = std::make_unique<char[]>(20);
                UDPC::preparePacket(
                    buf.get(),
                    c->protocolID,
                    0,
                    0,
                    0xFFFFFFFF,
                    nullptr,
                    0x1);

                struct sockaddr_in destinationInfo;
                destinationInfo.sin_family = AF_INET;
                destinationInfo.sin_addr.s_addr = iter->second.addr;
                destinationInfo.sin_port = htons(iter->second.port);
                long int sentBytes = sendto(
                    c->socketHandle,
                    buf.get(),
                    20,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(struct sockaddr_in));
                if(sentBytes != 20) {
                    c->log(
                        UDPC_LoggingType::ERROR,
                        "Failed to send packet to initiate connection to ",
                        UDPC_atostr(ctx, iter->second.addr),
                        ":",
                        iter->second.port);
                    continue;
                }
            } else {
                // is server, initiate connection to client
                iter->second.flags.reset(3);
                iter->second.sent = now;

                std::unique_ptr<char[]> buf = std::make_unique<char[]>(20);
                UDPC::preparePacket(
                    buf.get(),
                    c->protocolID,
                    iter->second.id,
                    iter->second.rseq,
                    iter->second.ack,
                    &iter->second.lseq,
                    0x1);

                struct sockaddr_in destinationInfo;
                destinationInfo.sin_family = AF_INET;
                destinationInfo.sin_addr.s_addr = iter->second.addr;
                destinationInfo.sin_port = htons(iter->second.port);
                long int sentBytes = sendto(
                    c->socketHandle,
                    buf.get(),
                    20,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(struct sockaddr_in));
                if(sentBytes != 20) {
                    c->log(
                        UDPC_LoggingType::ERROR,
                        "Failed to send packet to initiate connection to ",
                        UDPC_atostr(ctx, iter->second.addr),
                        ":",
                        iter->second.port);
                    continue;
                }
            }
            continue;
        }

        // Not initiating connection, send as normal on current connection
        if(iter->second.sendPkts.empty() && iter->second.priorityPkts.empty()) {
            // nothing in queues, send heartbeat packet
            auto sentDT = now - iter->second.sent;
            if(sentDT < UDPC::HEARTBEAT_PKT_INTERVAL_DT) {
                continue;
            }

            std::unique_ptr<char[]> buf = std::make_unique<char[]>(20);
            UDPC::preparePacket(
                buf.get(),
                c->protocolID,
                iter->second.id,
                iter->second.rseq,
                iter->second.ack,
                &iter->second.lseq,
                0);

            struct sockaddr_in destinationInfo;
            destinationInfo.sin_family = AF_INET;
            destinationInfo.sin_addr.s_addr = iter->second.addr;
            destinationInfo.sin_port = htons(iter->second.port);
            long int sentBytes = sendto(
                c->socketHandle,
                buf.get(),
                20,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(struct sockaddr_in));
            if(sentBytes != 20) {
                c->log(
                    UDPC_LoggingType::ERROR,
                    "Failed to send heartbeat packet to ",
                    UDPC_atostr(ctx, iter->second.addr),
                    ":",
                    iter->second.port);
                continue;
            }

            UDPC_PacketInfo pInfo{{0}, 0, 0, 0, 0, 0, 0};
            pInfo.sender.addr = UDPC::LOCAL_ADDR;
            pInfo.receiver.addr = iter->first.addr;
            pInfo.sender.port = c->socketInfo.sin_port;
            pInfo.receiver.port = iter->second.port;
            *((uint32_t*)(pInfo.data + 8)) = iter->second.lseq - 1;

            iter->second.sentPkts.push_back(std::move(pInfo));
            iter->second.cleanupSentPkts();

            // store other pkt info
            UDPC::SentPktInfo::Ptr sentPktInfo = std::make_shared<UDPC::SentPktInfo>();
            sentPktInfo->id = iter->second.lseq - 1;
            iter->second.sentInfoMap.insert(std::make_pair(sentPktInfo->id, sentPktInfo));
        } else {
            // sendPkts or priorityPkts not empty
            UDPC_PacketInfo pInfo;
            bool isResending = false;
            if(!iter->second.priorityPkts.empty()) {
                // TODO verify getting struct copy is valid
                pInfo = iter->second.priorityPkts.top();
                iter->second.priorityPkts.pop();
                isResending = true;
            } else {
                pInfo = iter->second.sendPkts.top();
                iter->second.sendPkts.pop();
            }
            std::unique_ptr<char[]> buf = std::make_unique<char[]>(20 + pInfo.dataSize);
            UDPC::preparePacket(
                buf.get(),
                c->protocolID,
                iter->second.id,
                iter->second.rseq,
                iter->second.ack,
                &iter->second.lseq,
                (pInfo.flags & 0x4) | (isResending ? 0x8 : 0));
            std::memcpy(buf.get() + 20, pInfo.data, pInfo.dataSize);

            struct sockaddr_in destinationInfo;
            destinationInfo.sin_family = AF_INET;
            destinationInfo.sin_addr.s_addr = iter->second.addr;
            destinationInfo.sin_port = htons(iter->second.port);
            long int sentBytes = sendto(
                c->socketHandle,
                buf.get(),
                pInfo.dataSize + 20,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(struct sockaddr_in));
            if(sentBytes != 20 + pInfo.dataSize) {
                c->log(
                    UDPC_LoggingType::ERROR,
                    "Failed to send packet to ",
                    UDPC_atostr(ctx, iter->second.addr),
                    ":",
                    iter->second.port);
                continue;
            }

            if((pInfo.flags & 0x4) == 0) {
                // is check-received, store data in case packet gets lost
                UDPC_PacketInfo sentPInfo;
                std::memcpy(sentPInfo.data, buf.get(), 20 + pInfo.dataSize);
                sentPInfo.flags = 0;
                sentPInfo.dataSize = 20 + pInfo.dataSize;
                sentPInfo.sender.addr = UDPC::LOCAL_ADDR;
                sentPInfo.receiver.addr = iter->first.addr;
                sentPInfo.sender.port = c->socketInfo.sin_port;
                sentPInfo.receiver.port = iter->second.port;

                iter->second.sentPkts.push_back(std::move(pInfo));
                iter->second.cleanupSentPkts();
            } else {
                // is not check-received, only id stored in data array
                UDPC_PacketInfo sentPInfo;
                sentPInfo.flags = 0x4;
                sentPInfo.dataSize = 0;
                sentPInfo.sender.addr = UDPC::LOCAL_ADDR;
                sentPInfo.receiver.addr = iter->first.addr;
                sentPInfo.sender.port = c->socketInfo.sin_port;
                sentPInfo.receiver.port = iter->second.port;
                *((uint32_t*)(sentPInfo.data + 8)) = iter->second.lseq - 1;

                iter->second.sentPkts.push_back(std::move(pInfo));
                iter->second.cleanupSentPkts();
            }

            // store other pkt info
            UDPC::SentPktInfo::Ptr sentPktInfo = std::make_shared<UDPC::SentPktInfo>();
            sentPktInfo->id = iter->second.lseq - 1;
            iter->second.sentInfoMap.insert(std::make_pair(sentPktInfo->id, sentPktInfo));
        }
    }

    // receive packet
#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
    typedef int socklen_t;
#endif
    struct sockaddr_in receivedData;
    socklen_t receivedDataSize = sizeof(receivedData);
    int bytes = recvfrom(
        c->socketHandle,
        c->recvBuf,
        UDPC_PACKET_MAX_SIZE,
        0,
        (struct sockaddr*) &receivedData,
        &receivedDataSize);

    if(bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // no packet was received
        return;
    } else if(bytes < 20) {
        // packet size is too small, invalid packet
        c->log(
            UDPC_LoggingType::INFO,
            "Received packet is smaller than header, ignoring packet from ",
            UDPC_atostr(ctx, receivedData.sin_addr.s_addr),
            ":",
            receivedData.sin_port);
        return;
    }

    uint32_t temp = ntohl(*((uint32_t*)c->recvBuf));
    if(temp != c->protocolID) {
        // Invalid protocol id in packet
        c->log(
            UDPC_LoggingType::INFO,
            "Received packet has invalid protocol id, ignoring packet from ",
            UDPC_atostr(ctx, receivedData.sin_addr.s_addr),
            ":",
            receivedData.sin_port);
        return;
    }

    uint32_t conID = ntohl(*((uint32_t*)(c->recvBuf + 4)));
    uint32_t seqID = ntohl(*((uint32_t*)(c->recvBuf + 8)));
    uint32_t rseq = ntohl(*((uint32_t*)(c->recvBuf + 12)));
    uint32_t ack = htonl(*((uint32_t*)(c->recvBuf + 16)));

    bool isConnect = conID & UDPC_ID_CONNECT;
    bool isPing = conID & UDPC_ID_PING;
    bool isNotRecChecked = conID & UDPC_ID_NO_REC_CHK;
    bool isResending = conID & UDPC_ID_RESENDING;
    conID &= 0x0FFFFFFF;

    UDPC_ConnectionId identifier{receivedData.sin_addr.s_addr, ntohs(receivedData.sin_port)};

    if(isConnect && c->flags.test(2)) {
        // is connect packet and is accepting new connections
        if(!c->flags.test(1)
                && c->conMap.find(identifier) == c->conMap.end()) {
            // is receiving as server, connection did not already exist
            c->log(
                UDPC_LoggingType::VERBOSE,
                "Establishing connection with client ",
                UDPC_atostr(ctx, receivedData.sin_addr.s_addr),
                ":",
                receivedData.sin_port);
            UDPC::ConnectionData newConnection(true, c);
            newConnection.addr = receivedData.sin_addr.s_addr;
            newConnection.port = ntohs(receivedData.sin_port);

            c->idMap.insert(std::make_pair(newConnection.id, identifier));
            c->conMap.insert(std::make_pair(identifier, std::move(newConnection)));
            auto addrConIter = c->addrConMap.find(identifier.addr);
            if(addrConIter == c->addrConMap.end()) {
                auto insertResult = c->addrConMap.insert(
                    std::make_pair(
                        identifier.addr,
                        std::unordered_set<UDPC_ConnectionId, UDPC::ConnectionIdHasher>{}
                    ));
                assert(insertResult.second
                        && "Must successfully insert into addrConMap");
                addrConIter = insertResult.first;
            }
            addrConIter->second.insert(identifier);
            // TODO trigger event server established connection with client
        } else if (c->flags.test(1)) {
            // is client
            auto iter = c->conMap.find(identifier);
            if(iter == c->conMap.end() || !iter->second.flags.test(3)) {
                return;
            }
            iter->second.flags.reset(3);
            iter->second.id = conID;
            iter->second.flags.set(4);
            c->log(
                UDPC_LoggingType::VERBOSE,
                "Established connection with server ",
                UDPC_atostr(ctx, receivedData.sin_addr.s_addr),
                ":",
                receivedData.sin_port);
            // TODO trigger event client established connection with server
        }
        return;
    }

    auto iter = c->conMap.find(identifier);
    if(iter == c->conMap.end() || iter->second.flags.test(3)
            || !iter->second.flags.test(4) || iter->second.id != conID) {
        return;
    }
    else if(isPing) {
        iter->second.flags.set(0);
    }

    // packet is valid
    c->log(
        UDPC_LoggingType::INFO,
        "Received valid packet from ",
        UDPC_atostr(ctx, receivedData.sin_addr.s_addr),
        ":",
        receivedData.sin_port);

    // update rtt
    for(auto sentIter = iter->second.sentPkts.rbegin(); sentIter != iter->second.sentPkts.rend(); ++sentIter) {
        uint32_t id = ntohl(*((uint32_t*)(sentIter->data + 8)));
        if(id == rseq) {
            auto sentInfoIter = iter->second.sentInfoMap.find(id);
            assert(sentInfoIter != iter->second.sentInfoMap.end()
                    && "sentInfoMap should have known stored id");
            auto diff = now - sentInfoIter->second->sentTime;
            if(diff > iter->second.rtt) {
                iter->second.rtt += (diff - iter->second.rtt) / 10;
            } else {
                iter->second.rtt -= (iter->second.rtt - diff) / 10;
            }

            iter->second.flags.set(2, iter->second.rtt <= UDPC::GOOD_RTT_LIMIT);

            c->log(
                UDPC_LoggingType::INFO,
                "RTT: ",
                UDPC::durationToFSec(iter->second.rtt));
            break;
        }
    }

    iter->second.received = now;

    // check pkt timeout
    --rseq;
    for(; ack != 0; ack = ack << 1) {
        if((ack & 0x80000000) != 0) {
            --rseq;
            continue;
        }

        // pkt not received yet, find it in sent to check if it timed out
        for(auto sentIter = iter->second.sentPkts.rbegin(); sentIter != iter->second.sentPkts.rend(); ++sentIter) {
            uint32_t sentID = ntohl(*((uint32_t*)(sentIter->data + 8)));
            if(sentID == rseq) {
                if((sentIter->flags & 0x4) != 0 || (sentIter->flags & 0x8) != 0) {
                    // already resent or not rec-checked pkt
                    break;
                }
                auto sentInfoIter = iter->second.sentInfoMap.find(sentID);
                assert(sentInfoIter != iter->second.sentInfoMap.end()
                        && "Every entry in sentPkts must have a corresponding entry in sentInfoMap");
                auto duration = now - sentInfoIter->second->sentTime;
                if(duration > UDPC::PACKET_TIMEOUT_TIME) {
                    if(sentIter->dataSize <= 20) {
                        c->log(
                            UDPC_LoggingType::INFO,
                            "Timed out packet has no payload (probably "
                            "heartbeat packet), ignoring it");
                        sentIter->flags |= 0x8;
                        break;
                    }

                    UDPC_PacketInfo resendingData;
                    resendingData.dataSize = sentIter->dataSize - 20;
                    std::memcpy(resendingData.data, sentIter->data + 20, resendingData.dataSize);
                    resendingData.flags = 0;
                    iter->second.priorityPkts.push(resendingData);
                }
                break;
            }
        }

        --rseq;
    }

    // calculate sequence and ack
    bool isOutOfOrder = false;
    uint32_t diff = 0;
    if(seqID > iter->second.rseq) {
        diff = seqID - iter->second.rseq;
        if(diff <= 0x7FFFFFFF) {
            // sequence is more recent
            iter->second.rseq = seqID;
            iter->second.ack = (iter->second.ack >> diff) | 0x80000000;
        } else {
            // sequence is older, recalc diff
            diff = 0xFFFFFFFF - seqID + 1 + iter->second.rseq;
            if((iter->second.ack & (0x80000000 >> (diff - 1))) != 0) {
                // already received packet
                c->log(
                    UDPC_LoggingType::INFO,
                    "Received packet is already marked as received, ignoring it");
                return;
            }
            iter->second.ack |= 0x80000000 >> (diff - 1);
            isOutOfOrder = true;
        }
    } else if(seqID < iter->second.rseq) {
        diff = iter->second.rseq - seqID;
        if(diff <= 0x7FFFFFFF) {
            // sequence is older
            if((iter->second.ack & (0x80000000 >> (diff - 1))) != 0) {
                // already received packet
                c->log(
                    UDPC_LoggingType::INFO,
                    "Received packet is already marked as received, ignoring it");
                return;
            }
            iter->second.ack |= 0x80000000 >> (diff - 1);
            isOutOfOrder = true;
        } else {
            // sequence is more recent, recalc diff
            diff = 0xFFFFFFFF - iter->second.rseq + 1 + seqID;
            iter->second.rseq = seqID;
            iter->second.ack = (iter->second.ack >> diff) | 0x80000000;
        }
    } else {
        // already received packet
        c->log(
            UDPC_LoggingType::INFO,
            "Received packet is already marked as received, ignoring it");
        return;
    }

    if(isOutOfOrder) {
        c->log(
            UDPC_LoggingType::VERBOSE,
            "Received packet is out of order");
    }

    if(bytes > 20) {
        UDPC_PacketInfo recPktInfo;
        std::memcpy(recPktInfo.data, c->recvBuf, bytes);
        recPktInfo.dataSize = bytes;
        recPktInfo.flags =
            (isConnect ? 0x1 : 0)
            | (isPing ? 0x2 : 0)
            | (isNotRecChecked ? 0x4 : 0)
            | (isResending ? 0x8 : 0);
        recPktInfo.sender.addr = receivedData.sin_addr.s_addr;
        recPktInfo.receiver.addr = UDPC::LOCAL_ADDR;
        recPktInfo.sender.port = receivedData.sin_port;
        recPktInfo.receiver.port = c->socketInfo.sin_port;

        if(iter->second.receivedPkts.size() == iter->second.receivedPkts.capacity()) {
            c->log(
                UDPC_LoggingType::WARNING,
                "receivedPkts is full, removing oldest entry to make room");
            iter->second.receivedPkts.pop();
        }

        iter->second.receivedPkts.push(recPktInfo);
    } else if(bytes == 20) {
        c->log(
            UDPC_LoggingType::VERBOSE,
            "Received packet has no payload");
    }
}

void UDPC_client_initiate_connection(UDPC_HContext ctx, uint32_t addr, uint16_t port) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c || !c->flags.test(1)) {
        return;
    }

    UDPC::ConnectionData newCon(false, c);
    UDPC_ConnectionId identifier{addr, port};

    // TODO make thread safe by using mutex
    c->conMap.insert(std::make_pair(identifier, std::move(newCon)));
    auto addrConIter = c->addrConMap.find(addr);
    if(addrConIter == c->addrConMap.end()) {
        auto insertResult = c->addrConMap.insert(std::make_pair(
                addr,
                std::unordered_set<UDPC_ConnectionId, UDPC::ConnectionIdHasher>{}
            ));
        assert(insertResult.second);
        addrConIter = insertResult.first;
    }
    addrConIter->second.insert(identifier);
}

int UDPC_get_queue_send_available(UDPC_HContext ctx, uint32_t addr, uint16_t port) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    UDPC_ConnectionId identifier{addr, port};

    auto iter = c->conMap.find(identifier);
    if(iter != c->conMap.end()) {
        return iter->second.sendPkts.capacity() - iter->second.sendPkts.size();
    } else {
        return 0;
    }
}

void UDPC_queue_send(UDPC_HContext ctx, uint32_t destAddr, uint16_t destPort,
                     uint32_t isChecked, void *data, uint32_t size) {
    if(size == 0 || !data) {
        return;
    }

    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return;
    }

    UDPC_ConnectionId identifier{destAddr, destPort};

    auto iter = c->conMap.find(identifier);
    if(iter == c->conMap.end()) {
        c->log(
            UDPC_LoggingType::ERROR,
            "Failed to add packet to queue, no established connection "
            "with recipient");
        return;
    }

    UDPC_PacketInfo sendInfo;
    std::memcpy(sendInfo.data, data, size);
    sendInfo.dataSize = size;
    sendInfo.sender.addr = UDPC::LOCAL_ADDR;
    sendInfo.sender.port = c->socketInfo.sin_port;
    sendInfo.receiver.addr = destAddr;
    sendInfo.receiver.port = iter->second.port;
    sendInfo.flags = (isChecked ? 0x0 : 0x4);

    iter->second.sendPkts.push(sendInfo);
}

int UDPC_set_accept_new_connections(UDPC_HContext ctx, int isAccepting) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }
    return c->isAcceptNewConnections.exchange(isAccepting == 0 ? false : true);
}

int UDPC_drop_connection(UDPC_HContext ctx, uint32_t addr, uint16_t port) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    UDPC_ConnectionId identifier{addr, port};

    auto iter = c->conMap.find(identifier);
    if(iter != c->conMap.end()) {
        if(iter->second.flags.test(4)) {
            c->idMap.erase(iter->second.id);
        }
        auto addrConIter = c->addrConMap.find(addr);
        if(addrConIter != c->addrConMap.end()) {
            addrConIter->second.erase(identifier);
            if(addrConIter->second.empty()) {
                c->addrConMap.erase(addrConIter);
            }
        }
        c->conMap.erase(iter);
        return 1;
    }

    return 0;
}

int UDPC_drop_connection_addr(UDPC_HContext ctx, uint32_t addr) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }

    auto addrConIter = c->addrConMap.find(addr);
    if(addrConIter != c->addrConMap.end()) {
        for(auto identIter = addrConIter->second.begin();
                identIter != addrConIter->second.end();
                ++identIter) {
            auto conIter = c->conMap.find(*identIter);
            assert(conIter != c->conMap.end());
            if(conIter->second.flags.test(4)) {
                c->idMap.erase(conIter->second.id);
            }
            c->conMap.erase(conIter);
        }
        c->addrConMap.erase(addrConIter);
        return 1;
    }

    return 0;
}

uint32_t UDPC_set_protocol_id(UDPC_HContext ctx, uint32_t id) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }
    return c->protocolID.exchange(id);
}

UDPC_LoggingType set_logging_type(UDPC_HContext ctx, UDPC_LoggingType loggingType) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return UDPC_LoggingType::SILENT;
    }
    return static_cast<UDPC_LoggingType>(c->loggingType.exchange(loggingType));
}

UDPC_PacketInfo UDPC_get_received(UDPC_HContext ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return UDPC_PacketInfo{{0}, 0, 0, 0, 0, 0, 0};
    }
    // TODO impl
    return UDPC_PacketInfo{{0}, 0, 0, 0, 0, 0, 0};
}

const char *UDPC_atostr(UDPC_HContext ctx, uint32_t addr) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return nullptr;
    }
    int index = 0;
    for(int x = 0; x < 4; ++x) {
        unsigned char temp = (addr >> (x * 8)) & 0xFF;

        if(temp >= 100) {
            c->atostrBuf[index++] = '0' + temp / 100;
        }
        if(temp >= 10) {
            c->atostrBuf[index++] = '0' + ((temp / 10) % 10);
        }
        c->atostrBuf[index++] = '0' + temp % 10;

        if(x < 3) {
            c->atostrBuf[index++] = '.';
        }
    }
    c->atostrBuf[index] = 0;

    return c->atostrBuf;
}

uint32_t UDPC_strtoa(const char *addrStr) {
    uint32_t addr = 0;
    uint32_t temp = 0;
    uint32_t index = 0;
    while(*addrStr != 0) {
        if(*addrStr >= '0' && *addrStr <= '9') {
            temp *= 10;
            temp += *addrStr - '0';
        } else if(*addrStr == '.' && temp <= 0xFF && index < 3) {
            if(UDPC::isBigEndian()) {
                addr |= (temp << (24 - 8 * index++));
            } else {
                addr |= (temp << (8 * index++));
            }
            temp = 0;
        } else {
            return 0;
        }
        ++addrStr;
    }

    if(index == 3 && temp <= 0xFF) {
        if(UDPC::isBigEndian()) {
            addr |= temp;
        } else {
            addr |= temp << 24;
        }
        return addr;
    } else {
        return 0;
    }
}
