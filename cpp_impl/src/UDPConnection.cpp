#include "UDPConnection.h"
#include "UDPC_Defines.hpp"

UDPC::Context::Context(bool isThreaded) :
_contextIdentifier(UDPC_CONTEXT_IDENTIFIER),
flags(),
isAcceptNewConnections(true),
protocolID(UDPC_DEFAULT_PROTOCOL_ID),
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

bool UDPC::VerifyContext(void *ctx) {
    if(ctx == nullptr) {
        return false;
    }
    UDPC::Context* c = (UDPC::Context*) ctx;
    if(c->_contextIdentifier == UDPC_CONTEXT_IDENTIFIER) {
        return true;
    } else {
        return false;
    }
}

void* UDPC_init(uint16_t listenPort, uint32_t listenAddr, int isClient) {
    UDPC::Context *ctx = new UDPC::Context(false);

    return ctx;
}

void* UDPC_init_threaded_update(uint16_t listenPort, uint32_t listenAddr, int isClient) {
    UDPC::Context *ctx = new UDPC::Context(true);

    return ctx;
}

void UDPC_destroy(void *ctx) {
    if (UDPC::VerifyContext(ctx)) {
        delete (UDPC::Context*)ctx;
    }
}

void UDPC_update(void *ctx) {
    if(!UDPC::VerifyContext(ctx)) {
        return;
    }
    UDPC::Context *c = (UDPC::Context*)ctx;
    if(c->flags.test(0)) {
        // is threaded, update should not be called
        return;
    }

    // TODO impl
}

int UDPC_get_queue_send_available(void *ctx, uint32_t addr) {
    if(!UDPC::VerifyContext(ctx)) {
        return 0;
    }
    UDPC::Context *c = (UDPC::Context*)ctx;
    // TODO impl
    return 0;
}

void UDPC_queue_send(void *ctx, uint32_t destAddr, uint16_t destPort, uint32_t isChecked, void *data, uint32_t size) {
    if(!UDPC::VerifyContext(ctx)) {
        return;
    }
    UDPC::Context *c = (UDPC::Context*)ctx;
    // TODO impl
}

int UDPC_set_accept_new_connections(void *ctx, int isAccepting) {
    if(!UDPC::VerifyContext(ctx)) {
        return 0;
    }
    UDPC::Context *c = (UDPC::Context*)ctx;
    return c->isAcceptNewConnections.exchange(isAccepting == 0 ? false : true);
}

int UDPC_drop_connection(void *ctx, uint32_t addr, uint16_t port) {
    if(!UDPC::VerifyContext(ctx)) {
        return 0;
    }
    UDPC::Context *c = (UDPC::Context*)ctx;
    // TODO impl
    return 0;
}

uint32_t UDPC_set_protocol_id(void *ctx, uint32_t id) {
    if(!UDPC::VerifyContext(ctx)) {
        return 0;
    }
    UDPC::Context *c = (UDPC::Context*)ctx;
    return c->protocolID.exchange(id);
}

UDPC_LoggingType set_logging_type(void *ctx, UDPC_LoggingType loggingType) {
    if(!UDPC::VerifyContext(ctx)) {
        return static_cast<UDPC_LoggingType>(0);
    }
    UDPC::Context *c = (UDPC::Context*)ctx;
    return static_cast<UDPC_LoggingType>(c->loggingType.exchange(loggingType));
}

PacketInfo UDPC_get_received(void *ctx) {
    if(!UDPC::VerifyContext(ctx)) {
        return PacketInfo{{0}, 0, 0, 0, 0, 0};
    }
    UDPC::Context *c = (UDPC::Context*)ctx;
    // TODO impl
    return PacketInfo{{0}, 0, 0, 0, 0, 0};
}

const char* UDPC_atostr(void *ctx, uint32_t addr) {
    if(!UDPC::VerifyContext(ctx)) {
        return nullptr;
    }
    UDPC::Context *c = (UDPC::Context*)ctx;
    int index = 0;
    for(int x = 0; x < 4; ++x)
    {
        unsigned char temp = (addr >> (x * 8)) & 0xFF;

        if(temp >= 100)
        {
            c->atostrBuf[index++] = '0' + temp / 100;
        }
        if(temp >= 10)
        {
            c->atostrBuf[index++] = '0' + ((temp / 10) % 10);
        }
        c->atostrBuf[index++] = '0' + temp % 10;

        if(x < 3)
        {
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
    while(*addrStr != 0)
    {
        if(*addrStr >= '0' && *addrStr <= '9')
        {
            temp *= 10;
            temp += *addrStr - '0';
        }
        else if(*addrStr == '.' && temp <= 0xFF && index < 3)
        {
            addr |= (temp << (8 * index++));
            temp = 0;
        }
        else
        {
            return 0;
        }
        ++addrStr;
    }

    if(index == 3 && temp <= 0xFF)
    {
        addr |= temp << 24;
        return addr;
    }
    else
    {
        return 0;
    }
}
