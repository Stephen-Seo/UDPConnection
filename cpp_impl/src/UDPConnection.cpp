#include "UDPC_Defines.hpp"
#include "UDPConnection.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <optional>
#include <vector>

UDPC::ConnectionData::ConnectionData() :
flags(),
sentPkts(),
sendPkts(UDPC_QUEUED_PKTS_MAX_SIZE),
priorityPkts(UDPC_QUEUED_PKTS_MAX_SIZE),
received(std::chrono::steady_clock::now()),
sent(std::chrono::steady_clock::now())
{
}

UDPC::ConnectionData::ConnectionData(bool isServer, Context *ctx) :
UDPC::ConnectionData::ConnectionData()
{
    if(isServer) {
        flags.set(0);
        flags.set(3);
        id = UDPC::generateConnectionID(*ctx);
        flags.set(4);
    }
}

UDPC::Context::Context(bool isThreaded)
    : _contextIdentifier(UDPC_CONTEXT_IDENTIFIER), flags(),
      isAcceptNewConnections(true), protocolID(UDPC_DEFAULT_PROTOCOL_ID),
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

UDPC::Context *UDPC::verifyContext(void *ctx) {
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

void *UDPC_init(uint16_t listenPort, uint32_t listenAddr, int isClient) {
    UDPC::Context *ctx = new UDPC::Context(false);

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

    return ctx;
}

void *UDPC_init_threaded_update(uint16_t listenPort, uint32_t listenAddr,
                                int isClient) {
    UDPC::Context *ctx =
        (UDPC::Context *)UDPC_init(listenPort, listenAddr, isClient);
    if(!ctx) {
        return nullptr;
    }
    ctx->flags.set(0);

    return ctx;
}

void UDPC_destroy(void *ctx) {
    UDPC::Context *UDPC_ctx = UDPC::verifyContext(ctx);
    if(UDPC_ctx) {
        delete UDPC_ctx;
    }
}

void UDPC_update(void *ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c || c->flags.test(0)) {
        // invalid or is threaded, update should not be called
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto dt = now - c->lastUpdated;
    const float dt_fs = (float)dt.count() * (float)decltype(dt)::period::num /
                        (float)decltype(dt)::period::den;

    std::chrono::steady_clock::duration temp_dt;
    float temp_dt_fs;
    {
        // check timed out, check good/bad mode with rtt, remove timed out
        std::vector<uint32_t> removed;
        for(auto iter = c->conMap.begin(); iter != c->conMap.end(); ++iter) {
            temp_dt = now - iter->second.received;
            temp_dt_fs = (float)temp_dt.count() *
                         (float)decltype(temp_dt)::period::num /
                         (float)decltype(temp_dt)::period::den;
            if(temp_dt_fs >= UDPC_TIMEOUT_SECONDS) {
                removed.push_back(iter->first);
                continue;
                // TODO log timed out connection
            }

            // check good/bad mode
            iter->second.toggleTimer += temp_dt_fs;
            iter->second.toggledTimer += temp_dt_fs;
            if(iter->second.flags.test(1) && !iter->second.flags.test(2)) {
                // good mode, bad rtt
                // TODO log switching to bad mode
                iter->second.flags.reset(1);
                if(iter->second.toggledTimer <= 10.0f) {
                    iter->second.toggleT *= 2.0f;
                }
                iter->second.toggledTimer = 0.0f;
            } else if(iter->second.flags.test(1)) {
                // good mode, good rtt
                if(iter->second.toggleTimer >= 10.0f) {
                    iter->second.toggleTimer = 0.0f;
                    iter->second.toggleT /= 2.0f;
                    if(iter->second.toggleT < 1.0f) {
                        iter->second.toggleT = 1.0f;
                    }
                }
            } else if(!iter->second.flags.test(1) &&
                      iter->second.flags.test(2)) {
                // bad mode, good rtt
                if(iter->second.toggledTimer >= iter->second.toggleT) {
                    iter->second.toggleTimer = 0.0f;
                    iter->second.toggledTimer = 0.0f;
                    // TODO log switching to good mode
                    iter->second.flags.set(1);
                }
            } else {
                // bad mode, bad rtt
                iter->second.toggledTimer = 0.0f;
            }

            iter->second.timer += temp_dt_fs;
            if(iter->second.timer >= (iter->second.flags.test(1)
                                          ? UDPC_GOOD_MODE_SEND_INTERVAL
                                          : UDPC_BAD_MODE_SEND_INTERVAL)) {
                iter->second.timer = 0.0f;
                iter->second.flags.set(0);
            }
        }
        for(auto iter = removed.begin(); iter != removed.end(); ++iter) {
            auto cIter = c->conMap.find(*iter);
            assert(cIter != c->conMap.end());
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
                destinationInfo.sin_addr.s_addr = iter->first;
                destinationInfo.sin_port = htons(iter->second.port);
                long int sentBytes = sendto(
                    c->socketHandle,
                    buf.get(),
                    20,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(struct sockaddr_in));
                if(sentBytes != 20) {
                    // TODO log fail of sending connection-initiate-packet
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
                destinationInfo.sin_addr.s_addr = iter->first;
                destinationInfo.sin_port = htons(iter->second.port);
                long int sentBytes = sendto(
                    c->socketHandle,
                    buf.get(),
                    20,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(struct sockaddr_in));
                if(sentBytes != 20) {
                    // TODO log fail send init connection packet as server
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
            destinationInfo.sin_addr.s_addr = iter->first;
            destinationInfo.sin_port = htons(iter->second.port);
            long int sentBytes = sendto(
                c->socketHandle,
                buf.get(),
                20,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(struct sockaddr_in));
            if(sentBytes != 20) {
                // TODO log fail send heartbeat packet
            }

            UDPC_PacketInfo pInfo{{0}, 0, 0, 0, 0, 0, 0};
            pInfo.sender = UDPC::LOCAL_ADDR;
            pInfo.receiver = iter->first;
            pInfo.senderPort = c->socketInfo.sin_port;
            pInfo.receiverPort = iter->second.port;

            iter->second.sentPkts.push_back(std::move(pInfo));
            while(iter->second.sentPkts.size() > UDPC_SENT_PKTS_MAX_SIZE) {
                iter->second.sentPkts.pop_front();
            }
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
                pInfo.flags & 0xC);
            std::memcpy(buf.get() + 20, pInfo.data, pInfo.dataSize);

            struct sockaddr_in destinationInfo;
            destinationInfo.sin_family = AF_INET;
            destinationInfo.sin_addr.s_addr = iter->first;
            destinationInfo.sin_port = htons(iter->second.port);
            long int sentBytes = sendto(
                c->socketHandle,
                buf.get(),
                pInfo.dataSize + 20,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(struct sockaddr_in));
            if(sentBytes != 20 + pInfo.dataSize) {
                // TODO log fail send packet
            }

            if((pInfo.flags & 0x4) == 0) {
                // is check-received, store data in case packet gets lost
                UDPC_PacketInfo sentPInfo;
                std::memcpy(sentPInfo.data, buf.get(), 20 + pInfo.dataSize);
                sentPInfo.flags = 0;
                sentPInfo.dataSize = 20 + pInfo.dataSize;
                sentPInfo.sender = UDPC::LOCAL_ADDR;
                sentPInfo.receiver = iter->first;
                sentPInfo.senderPort = c->socketInfo.sin_port;
                sentPInfo.receiverPort = iter->second.port;

                iter->second.sentPkts.push_back(std::move(pInfo));
                while(iter->second.sentPkts.size() > UDPC_SENT_PKTS_MAX_SIZE) {
                    iter->second.sentPkts.pop_front();
                }
            } else {
                // is not check-received, no data stored but other data is kept
                UDPC_PacketInfo sentPInfo;
                sentPInfo.flags = 0x4;
                sentPInfo.dataSize = 0;
                sentPInfo.sender = UDPC::LOCAL_ADDR;
                sentPInfo.receiver = iter->first;
                sentPInfo.senderPort = c->socketInfo.sin_port;
                sentPInfo.receiverPort = iter->second.port;

                iter->second.sentPkts.push_back(std::move(pInfo));
                while(iter->second.sentPkts.size() > UDPC_SENT_PKTS_MAX_SIZE) {
                    iter->second.sentPkts.pop_front();
                }
            }
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
        // TODO log this
        return;
    }

    uint32_t temp = ntohl(*((uint32_t*)c->recvBuf));
    if(temp != c->protocolID) {
        // Invalid protocol id in packet
        // TODO log this
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

    if(isConnect && c->flags.test(2)) {
        // is connect packet and is accepting new connections
        if(!c->flags.test(1)
                && c->conMap.find(receivedData.sin_addr.s_addr) == c->conMap.end()) {
            // is receiving as server, connection did not already exist
            // TODO log establishing connection with client peer
            UDPC::ConnectionData newConnection(true, c);
            // TODO update idMap and conMap with new CD
            // TODO impl establish connection with client peer as server
        }
    }

    // TODO impl
}

int UDPC_get_queue_send_available(void *ctx, uint32_t addr) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }
    // TODO impl
    return 0;
}

void UDPC_queue_send(void *ctx, uint32_t destAddr, uint16_t destPort,
                     uint32_t isChecked, void *data, uint32_t size) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return;
    }
    // TODO impl
}

int UDPC_set_accept_new_connections(void *ctx, int isAccepting) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }
    return c->isAcceptNewConnections.exchange(isAccepting == 0 ? false : true);
}

int UDPC_drop_connection(void *ctx, uint32_t addr, uint16_t port) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }
    // TODO impl
    return 0;
}

uint32_t UDPC_set_protocol_id(void *ctx, uint32_t id) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return 0;
    }
    return c->protocolID.exchange(id);
}

UDPC_LoggingType set_logging_type(void *ctx, UDPC_LoggingType loggingType) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return static_cast<UDPC_LoggingType>(0);
    }
    return static_cast<UDPC_LoggingType>(c->loggingType.exchange(loggingType));
}

UDPC_PacketInfo UDPC_get_received(void *ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return UDPC_PacketInfo{{0}, 0, 0, 0, 0, 0, 0};
    }
    // TODO impl
    return UDPC_PacketInfo{{0}, 0, 0, 0, 0, 0, 0};
}

const char *UDPC_atostr(void *ctx, uint32_t addr) {
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
