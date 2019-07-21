#include "UDPC_Defines.hpp"
#include "UDPConnection.h"

#include <chrono>
#include <optional>
#include <vector>

UDPC::Context::Context(bool isThreaded)
    : _contextIdentifier(UDPC_CONTEXT_IDENTIFIER), flags(),
      isAcceptNewConnections(true), protocolID(UDPC_DEFAULT_PROTOCOL_ID),
#ifndef NDEBUG
      loggingType(INFO)
#else
      loggingType(WARNING)
#endif
{
    if(isThreaded) {
        flags.set(0);
    } else {
        flags.reset(0);
    }
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
    std::vector<uint32_t> removed;
    for(auto iter = c->conMap.begin(); iter != c->conMap.end(); ++iter) {
        temp_dt = now - iter->second.received;
        temp_dt_fs = (float)temp_dt.count() *
                     (float)decltype(temp_dt)::period::num /
                     (float)decltype(temp_dt)::period::den;
        if(temp_dt_fs >= UDPC_TIMEOUT_SECONDS) {
            removed.push_back(iter->first);
            // TODO log timed out connection
        }

        // check good/bad mode
        iter->second.toggleTimer += temp_dt_fs;
        iter->second.toggledTimer += temp_dt_fs;
        if(iter->second.flags.test(1) && !iter->second.flags.test(2)) {
            // good mode, bad rtt
            // TODO
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

PacketInfo UDPC_get_received(void *ctx) {
    UDPC::Context *c = UDPC::verifyContext(ctx);
    if(!c) {
        return PacketInfo{{0}, 0, 0, 0, 0, 0};
    }
    // TODO impl
    return PacketInfo{{0}, 0, 0, 0, 0, 0};
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
