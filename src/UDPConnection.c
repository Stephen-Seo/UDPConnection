#include "UDPC_Defines.h"
#include "UDPConnection.h"

const char *UDPC_ERR_SOCKETFAIL_STR = "Failed to create socket";
const char *UDPC_ERR_SOCKETBINDF_STR = "Failed to bind socket";
const char *UDPC_ERR_SOCKETNONBF_STR = "Failed to set non-blocking on socket";
const char *UDPC_ERR_MTXFAIL_STR = "Failed to create mutex";
const char *UDPC_ERR_CVFAIL_STR = "Failed to create condition variable";
const char *UDPC_ERR_THREADFAIL_STR = "Failed to create thread";

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

UDPC_Context* UDPC_init(uint16_t listenPort, uint32_t listenAddr, int isClient)
{
    UDPC_Context *context = malloc(sizeof(UDPC_Context));
    context->isThreaded = 0;
    context->protocolID = UDPC_PKT_DEFAULT_PROTOCOL_ID;
    context->error = UDPC_SUCCESS;
    context->flags = 0x4C;
    if(isClient != 0) context->flags |= 0x2;
    context->threadFlags = 0;
    context->conMap = UDPC_HashMap_init(13, sizeof(UDPC_INTERNAL_ConnectionData));
    context->idMap = UDPC_HashMap_init(13, sizeof(UDPC_INTERNAL_ConnectionData*));
    timespec_get(&context->lastUpdated, TIME_UTC);
    context->atostrBuf[UDPC_ATOSTR_BUF_SIZE - 1] = 0;
    context->connectedEvents = UDPC_Deque_init(
        UDPC_CONNECTED_EVENT_SIZE * 4);
    context->disconnectedEvents = UDPC_Deque_init(
        UDPC_DISCONNECTED_EVENT_SIZE * 4);
    context->receivedPackets = UDPC_Deque_init(
        UDPC_REC_PKTS_ALLOC_SIZE * sizeof(UDPC_INTERNAL_PacketInfo));
    context->callbackConnected = NULL;
    context->callbackConnectedUserData = NULL;
    context->callbackDisconnected = NULL;
    context->callbackDisconnectedUserData = NULL;
    context->callbackReceived = NULL;
    context->callbackReceivedUserData = NULL;

    // seed rand
    srand(context->lastUpdated.tv_sec);

    // create socket
    context->socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(context->socketHandle <= 0)
    {
        context->socketHandle = 0;
        context->error = UDPC_ERR_SOCKETFAIL;
        UDPC_INTERNAL_log(context, 0, "Failed to create socket");
        return context;
    }

    // bind socket
    context->socketInfo.sin_family = AF_INET;
    context->socketInfo.sin_addr.s_addr =
        (listenAddr == 0 ? INADDR_ANY : listenAddr);
    context->socketInfo.sin_port = htons(listenPort);
    if(bind(
            context->socketHandle,
            (const struct sockaddr*) &context->socketInfo,
            sizeof(struct sockaddr_in)
        ) < 0)
    {
        context->error = UDPC_ERR_SOCKETBINDF;
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        UDPC_INTERNAL_log(context, 0, "Failed to bind socket");
        return context;
    }

    // set nonblocking on socket
#if UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
    int nonblocking = 1;
    if(fcntl(context->socketHandle, F_SETFL, O_NONBLOCK, nonblocking) == -1)
    {
#elif UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
    DWORD nonblocking = 1;
    if(ioctlsocket(context->socketHandle, FIONBIO, &nonblocking) != 0)
    {
#else
    {
#endif
        context->error = UDPC_ERR_SOCKETNONBF;
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        UDPC_INTERNAL_log(context, 0, "Failed to set non-blocking on socket");
#if UDPC_PLATFORM == UDPC_PLATFORM_UNKNOWN
        UDPC_INTERNAL_log(context, 0, "(Unknown platform)");
#endif
        return context;
    }

    return context;
}

UDPC_Context* UDPC_init_threaded_update(uint16_t listenPort, uint32_t listenAddr, int isClient)
{
    UDPC_Context *context = UDPC_init(listenPort, listenAddr, isClient);

    context->isThreaded = 1;

    context->error = mtx_init(&context->tCVMtx, mtx_timed);
    if(context->error != thrd_success)
    {
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        UDPC_INTERNAL_log(context, 0, "Failed to create mutex");
        context->error = UDPC_ERR_MTXFAIL;
        return context;
    }
    context->error = UDPC_SUCCESS;

    context->error = mtx_init(&context->tflagsMtx, mtx_timed);
    if(context->error != thrd_success)
    {
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        mtx_destroy(&context->tCVMtx);
        UDPC_INTERNAL_log(context, 0, "Failed to create mutex");
        context->error = UDPC_ERR_MTXFAIL;
        return context;
    }
    context->error = UDPC_SUCCESS;

    context->error = cnd_init(&context->threadCV);
    if(context->error != thrd_success)
    {
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        mtx_destroy(&context->tCVMtx);
        mtx_destroy(&context->tflagsMtx);
        UDPC_INTERNAL_log(context, 0, "Failed to create condition variable");
        context->error = UDPC_ERR_CVFAIL;
        return context;
    }
    context->error = UDPC_SUCCESS;

    context->error = thrd_create(
        &context->threadHandle, UDPC_INTERNAL_threadfn, context);
    if(context->error != thrd_success)
    {
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        mtx_destroy(&context->tCVMtx);
        mtx_destroy(&context->tflagsMtx);
        cnd_destroy(&context->threadCV);
        UDPC_INTERNAL_log(context, 0, "Failed to create thread");
        context->error = UDPC_ERR_THREADFAIL;
        return context;
    }
    context->error = UDPC_SUCCESS;

    return context;
}

void UDPC_destroy(UDPC_Context *ctx)
{
    if(ctx->isThreaded != 0)
    {
        mtx_lock(&ctx->tflagsMtx);
        ctx->threadFlags |= 0x1;
        mtx_unlock(&ctx->tflagsMtx);
        cnd_broadcast(&ctx->threadCV);

        thrd_join(ctx->threadHandle, NULL);

        mtx_destroy(&ctx->tCVMtx);
        mtx_destroy(&ctx->tflagsMtx);
        cnd_destroy(&ctx->threadCV);
    }

    CleanupSocket(ctx->socketHandle);
    UDPC_HashMap_itercall(ctx->conMap, UDPC_INTERNAL_destroy_conMap, NULL);
    UDPC_HashMap_destroy(ctx->conMap);
    UDPC_HashMap_destroy(ctx->idMap);

    UDPC_Deque_destroy(ctx->connectedEvents);
    UDPC_Deque_destroy(ctx->disconnectedEvents);

    while(ctx->receivedPackets->size != 0)
    {
        UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_get_front_ptr(ctx->receivedPackets, sizeof(UDPC_INTERNAL_PacketInfo));
        if(pinfo->data) { free(pinfo->data); }
        UDPC_Deque_pop_front(ctx->receivedPackets, sizeof(UDPC_INTERNAL_PacketInfo));
    }
    UDPC_Deque_destroy(ctx->receivedPackets);

    free(ctx);
}

void UDPC_INTERNAL_destroy_conMap(void *unused, uint32_t addr, char *data)
{
    UDPC_INTERNAL_ConnectionData *cd = (UDPC_INTERNAL_ConnectionData*)data;

    for(int x = 0; x * sizeof(UDPC_INTERNAL_PacketInfo) < cd->sentPkts->size; ++x)
    {
        UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_index_ptr(
            cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo), x);
        if(pinfo->data)
        {
            free(pinfo->data);
        }
    }
    UDPC_Deque_destroy(cd->sentPkts);

    for(int x = 0; x * sizeof(UDPC_INTERNAL_PacketInfo) < cd->sendPktQueue->size; ++x)
    {
        UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_index_ptr(
            cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo), x);
        if(pinfo->data)
        {
            free(pinfo->data);
        }
    }
    UDPC_Deque_destroy(cd->sendPktQueue);

    for(int x = 0; x * sizeof(UDPC_INTERNAL_PacketInfo) < cd->priorityPktQueue->size; ++x)
    {
        UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_index_ptr(
            cd->priorityPktQueue, sizeof(UDPC_INTERNAL_PacketInfo), x);
        if(pinfo->data)
        {
            free(pinfo->data);
        }
    }
    UDPC_Deque_destroy(cd->priorityPktQueue);
}

void UDPC_set_callback_connected(
    UDPC_Context *ctx, UDPC_callback_connected fptr, void *userData)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    ctx->callbackConnected = fptr;
    ctx->callbackConnectedUserData = userData;

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
}

void UDPC_set_callback_disconnected(
    UDPC_Context *ctx, UDPC_callback_disconnected fptr, void *userData)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    ctx->callbackDisconnected = fptr;
    ctx->callbackDisconnectedUserData = userData;

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
}

void UDPC_set_callback_received(
    UDPC_Context *ctx, UDPC_callback_received fptr, void *userData)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    ctx->callbackReceived = fptr;
    ctx->callbackReceivedUserData = userData;

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
}

void UDPC_check_events(UDPC_Context *ctx)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    if(ctx->callbackConnected)
    {
        for(int x = 0; x * 4 < ctx->connectedEvents->size; ++x)
        {
            ctx->callbackConnected(ctx->callbackConnectedUserData,
                *((uint32_t*)UDPC_Deque_index_ptr(ctx->connectedEvents, 4, x)));
        }
        UDPC_Deque_clear(ctx->connectedEvents);
    }
    else
    {
        UDPC_INTERNAL_log(ctx, 0, "Connected callback not set");
        UDPC_Deque_clear(ctx->connectedEvents);
    }

    if(ctx->callbackDisconnected)
    {
        for(int x = 0; x * 4 < ctx->disconnectedEvents->size; ++x)
        {
            ctx->callbackDisconnected(ctx->callbackDisconnectedUserData,
                *((uint32_t*)UDPC_Deque_index_ptr(ctx->disconnectedEvents, 4, x)));
        }
        UDPC_Deque_clear(ctx->disconnectedEvents);
    }
    else
    {
        UDPC_INTERNAL_log(ctx, 0, "Disconnected callback not set");
        UDPC_Deque_clear(ctx->disconnectedEvents);
    }

    if(ctx->callbackReceived)
    {
        for(int x = 0; x * sizeof(UDPC_INTERNAL_PacketInfo) < ctx->receivedPackets->size; ++x)
        {
            UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_index_ptr(
                ctx->receivedPackets, sizeof(UDPC_INTERNAL_PacketInfo), x);
            ctx->callbackReceived(
                ctx->callbackReceivedUserData,
                pinfo->addr,
                pinfo->data,
                pinfo->size);
            free(pinfo->data);
        }
        UDPC_Deque_clear(ctx->receivedPackets);
    }
    else
    {
        UDPC_INTERNAL_log(ctx, 0, "Received callback not set");
        for(int x = 0; x * sizeof(UDPC_INTERNAL_PacketInfo) < ctx->receivedPackets->size; ++x)
        {
            UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_index_ptr(
                ctx->receivedPackets, sizeof(UDPC_INTERNAL_PacketInfo), x);
            free(pinfo->data);
        }
        UDPC_Deque_clear(ctx->receivedPackets);
    }

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
}

void UDPC_client_initiate_connection(UDPC_Context *ctx, uint32_t addr, uint16_t port)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    if((ctx->flags & 0x2) == 0 || UDPC_HashMap_has(ctx->conMap, addr) != 0)
    {
        // must be client or no already-existing connection to same address
        if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
        return;
    }

    UDPC_INTERNAL_ConnectionData cd = {
        0x9,
        0,
        1,
        0,
        0xFFFFFFFF,
        0.0f,
        30.0f,
        0.0f,
        0.0f,
        addr,
        port,
        UDPC_Deque_init(sizeof(UDPC_INTERNAL_PacketInfo) * UDPC_SENT_PKTS_ALLOC_SIZE),
        UDPC_Deque_init(sizeof(UDPC_INTERNAL_PacketInfo) * UDPC_SEND_PKTS_ALLOC_SIZE),
        UDPC_Deque_init(sizeof(UDPC_INTERNAL_PacketInfo) * UDPC_RESEND_PKTS_ALLOC_SIZE),
        {0, 0},
        {0, 0},
        0.0f
    };

    timespec_get(&cd.received, TIME_UTC);
    cd.sent = cd.received;
    cd.sent.tv_sec -= UDPC_INIT_PKT_INTERVAL + 1;

    UDPC_HashMap_insert(ctx->conMap, addr, &cd);

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
}

int UDPC_queue_send(UDPC_Context *ctx, uint32_t addr, uint32_t isChecked, void *data, uint32_t size)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    UDPC_INTERNAL_ConnectionData *cd = UDPC_HashMap_get(ctx->conMap, addr);
    if(cd)
    {
        UDPC_INTERNAL_PacketInfo pinfo = {
            addr,
            0,
            isChecked != 0 ? 0x2 : 0,
            malloc(size),
            size,
            {0, 0}
        };
        if(pinfo.data)
        {
            memcpy(pinfo.data, data, size);
            if(UDPC_Deque_push_back(cd->sendPktQueue, &pinfo, sizeof(UDPC_INTERNAL_PacketInfo)) == 0)
            {
                UDPC_INTERNAL_log(ctx, 1, "Not enough free space in send "
                    "packet queue, failed to queue packet for sending");
                if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
                return 0;
            }
            else
            {
                if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
                return 1;
            }
        }
        else
        {
            UDPC_INTERNAL_log(ctx, 0, "Failed to allocate memory to new send-packet queue entry");
            if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
            return 0;
        }
    }
    else
    {
        UDPC_INTERNAL_log(ctx, 0, "Cannot send to %s when connection has not been esablished",
            UDPC_INTERNAL_atostr(ctx, addr));
        if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
        return 0;
    }
}

int UDPC_get_queue_send_available(UDPC_Context *ctx, uint32_t addr)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    UDPC_INTERNAL_ConnectionData *cd = UDPC_HashMap_get(ctx->conMap, addr);
    if(!cd)
    {
        if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
        return 0;
    }

    int available = UDPC_Deque_get_available(cd->sendPktQueue) / sizeof(UDPC_INTERNAL_PacketInfo);

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }

    return available;
}

int UDPC_get_accept_new_connections(UDPC_Context *ctx)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    int result = (ctx->flags & 0x40) != 0 ? 1 : 0;

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }

    return result;
}

void UDPC_set_accept_new_connections(UDPC_Context *ctx, int isAccepting)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    if(isAccepting != 0) { ctx->flags |= 0x40; }
    else { ctx->flags &= 0xFFFFFFBF; }

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
}

uint32_t UDPC_get_protocol_id(UDPC_Context *ctx)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    uint32_t id = ctx->protocolID;

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }

    return id;
}

void UDPC_set_protocol_id(UDPC_Context *ctx, uint32_t id)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    ctx->protocolID = id;

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
}

uint32_t UDPC_get_error(UDPC_Context *ctx)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    uint32_t error = ctx->error;
    ctx->error = 0;

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }

    return error;
}

const char* UDPC_get_error_str(uint32_t error)
{
    switch(error)
    {
    case UDPC_SUCCESS: return "No error";
    case UDPC_ERR_SOCKETFAIL: return UDPC_ERR_SOCKETFAIL_STR;
    case UDPC_ERR_SOCKETBINDF: return UDPC_ERR_SOCKETBINDF_STR;
    case UDPC_ERR_SOCKETNONBF: return UDPC_ERR_SOCKETNONBF_STR;
    case UDPC_ERR_MTXFAIL: return UDPC_ERR_MTXFAIL_STR;
    case UDPC_ERR_CVFAIL: return UDPC_ERR_CVFAIL_STR;
    case UDPC_ERR_THREADFAIL: return UDPC_ERR_THREADFAIL_STR;
    default: return "Unknown error";
    }
}

void UDPC_set_logging_type(UDPC_Context *ctx, uint32_t logType)
{
    if(ctx->isThreaded != 0) { mtx_lock(&ctx->tCVMtx); }

    switch(logType)
    {
    case 0:
        ctx->flags &= 0xFFFFFFC3;
        break;
    case 1:
        ctx->flags &= 0xFFFFFFC7;
        ctx->flags |= 0x4;
        break;
    case 2:
        ctx->flags &= 0xFFFFFFCF;
        ctx->flags |= (0x4 | 0x8);
        break;
    case 3:
        ctx->flags &= 0xFFFFFFDF;
        ctx->flags |= (0x4 | 0x8 | 0x10);
        break;
    default:
        ctx->flags |= (0x4 | 0x8 | 0x10 | 0x20);
        break;
    }

    if(ctx->isThreaded != 0) { mtx_unlock(&ctx->tCVMtx); }
}

void UDPC_update(UDPC_Context *ctx)
{
    UDPC_INTERNAL_update_struct us = {
        {0, 0},
        0.0f,
        NULL,
        ctx
    };
    timespec_get(&us.tsNow, TIME_UTC);
    us.dt = UDPC_INTERNAL_ts_diff(&us.tsNow, &ctx->lastUpdated);
    ctx->lastUpdated = us.tsNow;
    us.removedQueue = UDPC_Deque_init(4 * (ctx->conMap->size));

    UDPC_HashMap_itercall(ctx->conMap, UDPC_INTERNAL_update_to_rtt_si, &us);

    // remove timed out
    for(int x = 0; x * 4 < us.removedQueue->size; ++x)
    {
        uint32_t *key = UDPC_Deque_index_ptr(us.removedQueue, 4, x);
        UDPC_INTERNAL_ConnectionData *cd = UDPC_HashMap_get(ctx->conMap, *key);

        while(cd->sentPkts->size != 0)
        {
            UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_get_front_ptr(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo));
            if(pinfo->data) { free(pinfo->data); }
            UDPC_Deque_pop_front(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo));
        }
        UDPC_Deque_destroy(cd->sentPkts);

        while(cd->sendPktQueue->size != 0)
        {
            UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_get_front_ptr(cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
            if(pinfo->data) { free(pinfo->data); }
            UDPC_Deque_pop_front(cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
        }
        UDPC_Deque_destroy(cd->sendPktQueue);

        while(cd->priorityPktQueue->size != 0)
        {
            UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_get_front_ptr(cd->priorityPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
            if(pinfo->data) { free(pinfo->data); }
            UDPC_Deque_pop_front(cd->priorityPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
        }
        UDPC_Deque_destroy(cd->priorityPktQueue);

        if((cd->flags & 0x10) != 0)
        {
            UDPC_HashMap_remove(ctx->idMap, cd->id);
        }
        UDPC_HashMap_remove(ctx->conMap, *key);
    }
    UDPC_Deque_destroy(us.removedQueue);
    us.removedQueue = NULL;

    // check triggerSend to send packets to connected
    UDPC_HashMap_itercall(ctx->conMap, UDPC_INTERNAL_update_send, &us);

    // receive packet
#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
    typedef int socklen_t;
#endif
    struct sockaddr_in receivedData;
    socklen_t receivedDataSize = sizeof(receivedData);
    int bytes = recvfrom(
        ctx->socketHandle,
        ctx->recvBuf,
        UDPC_PACKET_MAX_SIZE,
        0,
        (struct sockaddr*) &receivedData,
        &receivedDataSize);

    if(bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        // expected result for non-blocking socket
        return;
    }

    if(bytes < 20)
    {
        UDPC_INTERNAL_log(ctx, 2, "Got invalid packet from %s port %d (too small)",
            UDPC_INTERNAL_atostr(ctx, receivedData.sin_addr.s_addr),
            ntohs(receivedData.sin_port));
        return;
    }

    uint32_t temp = ntohl(*((uint32_t*)ctx->recvBuf));
    if(temp != ctx->protocolID)
    {
        UDPC_INTERNAL_log(ctx, 2, "Got invalid packet from %s port %d (invalid protocol id)",
            UDPC_INTERNAL_atostr(ctx, receivedData.sin_addr.s_addr),
            ntohs(receivedData.sin_port));
        return;
    }

    uint32_t conID = ntohl(*((uint32_t*)(ctx->recvBuf + 4)));
    uint32_t seqID = ntohl(*((uint32_t*)(ctx->recvBuf + 8)));
    uint32_t rseq = ntohl(*((uint32_t*)(ctx->recvBuf + 12)));
    uint32_t ack = ntohl(*((uint32_t*)(ctx->recvBuf + 16)));

    int isConnect = conID & UDPC_ID_CONNECT;
    int isPing = conID & UDPC_ID_PING;
    int isNotRecvCheck = conID & UDPC_ID_NO_REC_CHK;
    int isResent = conID & UDPC_ID_RESENDING;
    conID &= 0x0FFFFFFF;

    if(isConnect != 0 && (ctx->flags & 0x40) != 0)
    {
        if((ctx->flags & 0x2) == 0
            && !UDPC_HashMap_get(ctx->conMap, receivedData.sin_addr.s_addr))
        {
            UDPC_INTERNAL_log(ctx, 2, "Establishing connection with %s port %d",
                UDPC_INTERNAL_atostr(ctx, receivedData.sin_addr.s_addr),
                ntohs(receivedData.sin_port));
            UDPC_INTERNAL_ConnectionData newCD = {
                0x19,
                UDPC_INTERNAL_generate_id(ctx),
                0,
                0,
                0xFFFFFFFF,
                0.0f,
                30.0f,
                0.0f,
                0.0f,
                receivedData.sin_addr.s_addr,
                ntohs(receivedData.sin_port),
                UDPC_Deque_init(sizeof(UDPC_INTERNAL_PacketInfo) * UDPC_SENT_PKTS_ALLOC_SIZE),
                UDPC_Deque_init(sizeof(UDPC_INTERNAL_PacketInfo) * UDPC_SEND_PKTS_ALLOC_SIZE),
                UDPC_Deque_init(sizeof(UDPC_INTERNAL_PacketInfo) * UDPC_RESEND_PKTS_ALLOC_SIZE),
                us.tsNow,
                us.tsNow,
                0.0f
            };
            UDPC_HashMap_insert(ctx->conMap, newCD.addr, &newCD);
            UDPC_HashMap_insert(ctx->idMap, newCD.id, UDPC_HashMap_get(ctx->conMap, newCD.addr));
            if(UDPC_Deque_get_available(ctx->connectedEvents) == 0)
            {
                UDPC_Deque_pop_front(ctx->connectedEvents, 4);
                UDPC_Deque_push_back(ctx->connectedEvents, &receivedData.sin_addr.s_addr, 4);
                UDPC_INTERNAL_log(ctx, 1, "Not enough free space in connected "
                    "events queue, removing oldest to make room");
            }
            else
            {
                UDPC_Deque_push_back(ctx->connectedEvents, &receivedData.sin_addr.s_addr, 4);
            }
        }
        else if((ctx->flags & 0x2) != 0)
        {
            UDPC_INTERNAL_ConnectionData *cd = UDPC_HashMap_get(ctx->conMap, receivedData.sin_addr.s_addr);
            if(!cd) { return; }

            cd->flags &= 0xFFFFFFF7;
            cd->flags |= 0x10;
            cd->id = conID;
            UDPC_INTERNAL_log(ctx, 2, "Got id %u from server %s", conID,
                UDPC_INTERNAL_atostr(ctx, receivedData.sin_addr.s_addr));
            if(UDPC_Deque_get_available(ctx->connectedEvents) == 0)
            {
                UDPC_Deque_pop_front(ctx->connectedEvents, 4);
                UDPC_Deque_push_back(ctx->connectedEvents, &receivedData.sin_addr.s_addr, 4);
                UDPC_INTERNAL_log(ctx, 1, "Not enough free space in connected "
                    "events queue, removing oldest to make room");
            }
            else
            {
                UDPC_Deque_push_back(ctx->connectedEvents, &receivedData.sin_addr.s_addr, 4);
            }
        }
        return;
    }
    else if(isPing != 0)
    {
        UDPC_INTERNAL_ConnectionData *cd = UDPC_HashMap_get(ctx->conMap, receivedData.sin_addr.s_addr);
        if(cd)
        {
            cd->flags |= 0x1;
        }
    }

    UDPC_INTERNAL_ConnectionData *cd = UDPC_HashMap_get(ctx->conMap, receivedData.sin_addr.s_addr);
    if(!cd)
    {
        // Received packet from unknown, ignoring
        return;
    }
    else if(conID != cd->id)
    {
        // Received packet id and known id does not match, ignoring
        return;
    }

    // packet is valid
    UDPC_INTERNAL_log(ctx, 2, "Valid packet %d from %s", seqID,
        UDPC_INTERNAL_atostr(ctx, receivedData.sin_addr.s_addr));

    UDPC_INTERNAL_update_rtt(ctx, cd, rseq, &us.tsNow);
    cd->received = us.tsNow;
    UDPC_INTERNAL_check_pkt_timeout(ctx, cd, rseq, ack, &us.tsNow);

    int isOutOfOrder = 0;
    uint32_t diff = 0;
    if(seqID > cd->rseq)
    {
        diff = seqID - cd->rseq;
        if(diff <= 0x7FFFFFFF)
        {
            // seqeuence is more recent
            cd->rseq = seqID;
            cd->ack = (cd->ack >> diff) | 0x80000000;
        }
        else
        {
            // sequence is older id, diff requires recalc
            diff = 0xFFFFFFFF - seqID + 1 + cd->rseq;
            if((cd->ack & (0x80000000 >> (diff - 1))) != 0)
            {
                // already received packet
                UDPC_INTERNAL_log(ctx, 2, "Ignoring already received pkt from %s",
                    UDPC_INTERNAL_atostr(ctx, cd->addr));
                return;
            }
            cd->ack |= (0x80000000 >> (diff - 1));

            isOutOfOrder = 1;
        }
    }
    else if(seqID < cd->rseq)
    {
        diff = cd->rseq - seqID;
        if(diff <= 0x7FFFFFFF)
        {
            // sequence is older
            if((cd->ack & (0x80000000 >> (diff - 1))) != 0)
            {
                // already received packet
                UDPC_INTERNAL_log(ctx, 2, "Ignoring already received pkt from %s",
                    UDPC_INTERNAL_atostr(ctx, cd->addr));
                return;
            }
            cd->ack |= (0x80000000 >> (diff - 1));

            isOutOfOrder = 1;
        }
        else
        {
            // sequence is more recent, diff requires recalc
            diff = 0xFFFFFFFF - cd->rseq + 1 + seqID;
            cd->rseq = seqID;
            cd->ack = (cd->ack >> diff) | 0x80000000;
        }
    }
    else
    {
        // already received packet
        UDPC_INTERNAL_log(ctx, 2, "Ignoring already received (duplicate) pkt from %s",
            UDPC_INTERNAL_atostr(ctx, cd->addr));
        return;
    }

    if(isOutOfOrder != 0)
    {
        UDPC_INTERNAL_log(ctx, 2, "Received valid packet from %s is out of order",
            UDPC_INTERNAL_atostr(ctx, cd->addr));
    }

    if(bytes > 20)
    {
        UDPC_INTERNAL_PacketInfo receivedInfo;
        receivedInfo.addr = receivedData.sin_addr.s_addr;
        receivedInfo.id = conID;
        receivedInfo.flags = (isNotRecvCheck != 0 ? 0 : 0x2) | (isResent != 0 ? 0x5 : 0);
        receivedInfo.data = malloc(bytes - 20);
        memcpy(receivedInfo.data, ctx->recvBuf + 20, bytes - 20);
        receivedInfo.size = bytes - 20;
        receivedInfo.sent = us.tsNow;

        if(UDPC_Deque_get_available(ctx->receivedPackets) == 0)
        {
            UDPC_INTERNAL_PacketInfo *rpinfo = UDPC_Deque_get_front_ptr(
                ctx->receivedPackets, sizeof(UDPC_INTERNAL_PacketInfo));
            if(rpinfo->data) { free(rpinfo->data); }
            UDPC_Deque_pop_front(ctx->receivedPackets, sizeof(UDPC_INTERNAL_PacketInfo));
            UDPC_Deque_push_back(ctx->receivedPackets, &receivedInfo, sizeof(UDPC_INTERNAL_PacketInfo));
            UDPC_INTERNAL_log(ctx, 1, "Received packet but not enough space in received queue, removing oldest packet to make room");
        }
        else
        {
            UDPC_Deque_push_back(ctx->receivedPackets, &receivedInfo, sizeof(UDPC_INTERNAL_PacketInfo));
        }
    }
    else
    {
        UDPC_INTERNAL_log(ctx, 3, "Packet has no payload, not adding to received queue");
    }
}

void UDPC_INTERNAL_update_to_rtt_si(void *userData, uint32_t addr, char *data)
{
    UDPC_INTERNAL_update_struct *us =
        (UDPC_INTERNAL_update_struct*)userData;
    UDPC_INTERNAL_ConnectionData *cd = (UDPC_INTERNAL_ConnectionData*)data;

    // check for timed out connection
    if(UDPC_INTERNAL_ts_diff(&us->tsNow, &cd->received) >= UDPC_TIMEOUT_SECONDS)
    {
        UDPC_Deque_push_back(us->removedQueue, &addr, 4);
        UDPC_INTERNAL_log(us->ctx, 2, "Connection timed out with addr %s port %d",
            UDPC_INTERNAL_atostr(us->ctx, addr),
            cd->port);
        if(UDPC_Deque_get_available(us->ctx->disconnectedEvents) == 0)
        {
            UDPC_Deque_pop_front(us->ctx->disconnectedEvents, 4);
            UDPC_Deque_push_back(us->ctx->disconnectedEvents, &addr, 4);
            UDPC_INTERNAL_log(us->ctx, 1, "Not enough free space in "
                "disconnected events queue, removing oldest event to make room");
        }
        else
        {
            UDPC_Deque_push_back(us->ctx->disconnectedEvents, &addr, 4);
        }
        return;
    }

    // check good/bad mode
    cd->toggleTimer += us->dt;
    cd->toggledTimer += us->dt;
    if((cd->flags & 0x2) != 0 && (cd->flags & 0x4) == 0)
    {
        // good mode, bad rtt
        UDPC_INTERNAL_log(us->ctx, 2, "Connection with %s switching to bad mode",
            UDPC_INTERNAL_atostr(us->ctx, addr));

        cd->flags = cd->flags & 0xFFFFFFFD;
        if(cd->toggledTimer <= 10.0f)
        {
            cd->toggleT *= 2.0f;
            if(cd->toggleT > 60.0f)
            {
                cd->toggleT = 60.0f;
            }
        }
        cd->toggledTimer = 0.0f;
    }
    else if((cd->flags & 0x2) != 0)
    {
        // good mode, good rtt
        if(cd->toggleTimer >= 10.0f)
        {
            cd->toggleTimer = 0.0f;
            cd->toggleT /= 2.0f;
            if(cd->toggleT < 1.0f)
            {
                cd->toggleT = 1.0f;
            }
        }
    }
    else if((cd->flags & 0x2) == 0 && (cd->flags & 0x4) != 0)
    {
        // bad mode, good rtt
        if(cd->toggledTimer >= cd->toggleT)
        {
            cd->toggleTimer = 0.0f;
            cd->toggledTimer = 0.0f;
            UDPC_INTERNAL_log(us->ctx, 2, "Connection with %s switching to good mode",
                UDPC_INTERNAL_atostr(us->ctx, addr));
            cd->flags |= 0x2;
        }
    }
    else
    {
        // bad mode, bad rtt
        cd->toggledTimer = 0.0f;
    }

    // check send interval
    cd->timer += us->dt;
    if(cd->timer >= ((cd->flags & 0x2) != 0
        ? UDPC_GOOD_MODE_SEND_INTERVAL : UDPC_BAD_MODE_SEND_INTERVAL))
    {
        cd->timer = 0.0f;
        cd->flags |= 0x1;
    }
}

void UDPC_INTERNAL_update_send(void *userData, uint32_t addr, char *data)
{
    UDPC_INTERNAL_update_struct *us =
        (UDPC_INTERNAL_update_struct*)userData;
    UDPC_INTERNAL_ConnectionData *cd = (UDPC_INTERNAL_ConnectionData*)data;

    if((cd->flags & 0x1) == 0)
    {
        return;
    }

    cd->flags = cd->flags & 0xFFFFFFFE;

    if((cd->flags & 0x8) != 0)
    {
        if((us->ctx->flags & 0x2) != 0)
        {
            // initiate connection to server
            if(UDPC_INTERNAL_ts_diff(&us->tsNow, &cd->sent) < UDPC_INIT_PKT_INTERVAL_F)
            {
                return;
            }
            cd->sent = us->tsNow;

            char *data = malloc(20);
            UDPC_INTERNAL_prepare_pkt(
                data,
                us->ctx->protocolID,
                0,
                0,
                0xFFFFFFFF,
                NULL,
                0x8);

            struct sockaddr_in destinationInfo;
            destinationInfo.sin_family = AF_INET;
            destinationInfo.sin_addr.s_addr = addr;
            destinationInfo.sin_port = htons(cd->port);
            long int sentBytes = sendto(
                us->ctx->socketHandle,
                data,
                20,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(struct sockaddr_in));
            if(sentBytes != 20)
            {
                UDPC_INTERNAL_log(us->ctx, 0, "Failed to send init packet to %s "
                    "port %d", UDPC_INTERNAL_atostr(us->ctx, addr), cd->port);
                free(data);
                return;
            }
            free(data);
        }
        else
        {
            // initiate connection to client
            cd->flags &= 0xFFFFFFF7;
            cd->sent = us->tsNow;

            char *data = malloc(20);
            UDPC_INTERNAL_prepare_pkt(
                data,
                us->ctx->protocolID,
                cd->id,
                cd->rseq,
                cd->ack,
                &cd->lseq,
                0x8);

            struct sockaddr_in destinationInfo;
            destinationInfo.sin_family = AF_INET;
            destinationInfo.sin_addr.s_addr = addr;
            destinationInfo.sin_port = htons(cd->port);
            long int sentBytes = sendto(
                us->ctx->socketHandle,
                data,
                20,
                0,
                (struct sockaddr*) &destinationInfo,
                sizeof(struct sockaddr_in));
            if(sentBytes != 20)
            {
                UDPC_INTERNAL_log(us->ctx, 0, "Failed to send init packet to %s "
                    "port %d", UDPC_INTERNAL_atostr(us->ctx, addr), cd->port);
                free(data);
                return;
            }
            free(data);
        }
        return;
    }

    if(cd->sendPktQueue->size == 0 && cd->priorityPktQueue->size == 0)
    {
        // send and resend packet queue is empty, send heartbeat packet
        if(UDPC_INTERNAL_ts_diff(&us->tsNow, &cd->sent) < UDPC_HEARTBEAT_PKT_INTERVAL)
        {
            return;
        }

        char *data = malloc(20);
        UDPC_INTERNAL_prepare_pkt(
            data, us->ctx->protocolID, cd->id, cd->rseq, cd->ack, &cd->lseq, 0);

        struct sockaddr_in destinationInfo;
        destinationInfo.sin_family = AF_INET;
        destinationInfo.sin_addr.s_addr = addr;
        destinationInfo.sin_port = htons(cd->port);
        long int sentBytes = sendto(
            us->ctx->socketHandle,
            data,
            20,
            0,
            (struct sockaddr*) &destinationInfo,
            sizeof(struct sockaddr_in));
        if(sentBytes != 20)
        {
            UDPC_INTERNAL_log(us->ctx, 0, "Failed to send packet to %s port %d",
                UDPC_INTERNAL_atostr(us->ctx, addr), cd->port);
            free(data);
            return;
        }

        UDPC_INTERNAL_PacketInfo sentInfo = {
            addr,
            cd->lseq - 1,
            0,
            NULL,
            0,
            us->tsNow
        };
        UDPC_Deque_push_back(
            cd->sentPkts, &sentInfo, sizeof(UDPC_INTERNAL_PacketInfo));
        while(cd->sentPkts->size / sizeof(UDPC_INTERNAL_PacketInfo) > UDPC_SENT_PKTS_MAX_SIZE)
        {
            UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_get_front_ptr(
                cd->sentPkts,
                sizeof(UDPC_INTERNAL_PacketInfo));
            if(pinfo->data && pinfo->size != 0)
            {
                free(pinfo->data);
            }
            UDPC_Deque_pop_front(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo));
        }
        free(data);
    }
    else // sendPktQueue or priorityPktQueue not empty
    {
        UDPC_INTERNAL_PacketInfo *pinfo;
        int isResendingPkt = 0;

        if(cd->priorityPktQueue->size != 0)
        {
            pinfo = UDPC_Deque_get_front_ptr(
                cd->priorityPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
            isResendingPkt = 1;
        }
        else
        {
            pinfo = UDPC_Deque_get_front_ptr(
                cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
        }
        char *data = malloc(20 + pinfo->size);
        UDPC_INTERNAL_prepare_pkt(
            data,
            us->ctx->protocolID,
            cd->id,
            cd->rseq,
            cd->ack,
            &cd->lseq,
            ((pinfo->flags & 0x3) << 1));
        memcpy(&data[20], pinfo->data, pinfo->size);

        struct sockaddr_in destinationInfo;
        destinationInfo.sin_family = AF_INET;
        destinationInfo.sin_addr.s_addr = addr;
        destinationInfo.sin_port = htons(cd->port);
        long int sentBytes = sendto(
            us->ctx->socketHandle,
            data,
            20 + pinfo->size,
            0,
            (struct sockaddr*) &destinationInfo,
            sizeof(struct sockaddr_in));
        if(sentBytes != 20 + pinfo->size)
        {
            UDPC_INTERNAL_log(us->ctx, 0, "Failed to send packet to %s port %d",
                UDPC_INTERNAL_atostr(us->ctx, addr), cd->port);
            free(data);
            free(pinfo->data);
            if(isResendingPkt != 0)
            {
                UDPC_Deque_pop_front(cd->priorityPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
            }
            else
            {
                UDPC_Deque_pop_front(cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
            }
            return;
        }

        if((pinfo->flags & 0x2) != 0)
        {
            UDPC_INTERNAL_PacketInfo sentInfo = {
                addr,
                cd->lseq - 1,
                0x2,
                data,
                20 + pinfo->size,
                us->tsNow
            };
            UDPC_Deque_push_back(
                cd->sentPkts, &sentInfo, sizeof(UDPC_INTERNAL_PacketInfo));
        }
        else
        {
            UDPC_INTERNAL_PacketInfo sentInfo = {
                addr,
                cd->lseq - 1,
                0,
                NULL,
                0,
                us->tsNow
            };
            UDPC_Deque_push_back(
                cd->sentPkts, &sentInfo, sizeof(UDPC_INTERNAL_PacketInfo));
            free(data);
        }

        while(cd->sentPkts->size / sizeof(UDPC_INTERNAL_PacketInfo) > UDPC_SENT_PKTS_MAX_SIZE)
        {
            UDPC_INTERNAL_PacketInfo *pinfoCached = UDPC_Deque_get_front_ptr(
                cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo));
            if(pinfoCached->data && pinfoCached->size != 0)
            {
                free(pinfoCached->data);
            }
            UDPC_Deque_pop_front(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo));
        }

        free(pinfo->data);
        if(isResendingPkt != 0)
        {
            UDPC_Deque_pop_front(cd->priorityPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
        }
        else
        {
            UDPC_Deque_pop_front(cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
        }
    }
}

void UDPC_INTERNAL_update_rtt(
    UDPC_Context *ctx,
    UDPC_INTERNAL_ConnectionData *cd,
    uint32_t rseq,
    struct timespec *tsNow)
{
    for(int x = 0; x * sizeof(UDPC_INTERNAL_PacketInfo) < cd->sentPkts->size; ++x)
    {
        UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_index_ptr(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo), x);
        if(pinfo->id == rseq)
        {
            float diff = UDPC_INTERNAL_ts_diff(tsNow, &pinfo->sent);
            if(diff > cd->rtt)
            {
                cd->rtt += (diff - cd->rtt) / 10.0f;
            }
            else
            {
                cd->rtt -= (cd->rtt - diff) / 10.0f;
            }

            if(cd->rtt > UDPC_GOOD_RTT_LIMIT_SEC)
            {
                cd->flags &= 0xFFFFFFFB;
            }
            else
            {
                cd->flags |= 0x4;
            }

            UDPC_INTERNAL_log(ctx, 2, "%d RTT (%s) %.3fs from %s",
                rseq,
                cd->rtt > UDPC_GOOD_RTT_LIMIT_SEC ? "B" : "G",
                cd->rtt,
                UDPC_INTERNAL_atostr(ctx, cd->addr));
            break;
        }
    }
}

void UDPC_INTERNAL_check_pkt_timeout(
    UDPC_Context *ctx,
    UDPC_INTERNAL_ConnectionData *cd,
    uint32_t rseq,
    uint32_t ack,
    struct timespec *tsNow)
{
    --rseq;
    for(; ack != 0; ack = ack << 1)
    {
        if((ack & 0x80000000) != 0) { --rseq; continue; }

        // not received by peer yet, check if packet timed out
        for(int x = 0; x * sizeof(UDPC_INTERNAL_PacketInfo) < cd->sentPkts->size; ++x)
        {
            UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_index_rev_ptr(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo), x);
            if(pinfo->id == rseq)
            {
                if((pinfo->flags & 0x2) == 0 || (pinfo->flags & 0x4) != 0)
                {
                    // is not received checked or already resent
                    break;
                }
                float seconds = UDPC_INTERNAL_ts_diff(tsNow, &pinfo->sent);
                if(seconds >= UDPC_PACKET_TIMEOUT_SEC)
                {
                    if(pinfo->size <= 20)
                    {
                        UDPC_INTERNAL_log(ctx, 0,
                            "Timed out sentPkt (%d) to %s has size at most 20",
                            rseq,
                            UDPC_INTERNAL_atostr(ctx, cd->addr));
                        pinfo->flags |= 0x4; // treat as resent to avoid reprinting error
                        break;
                    }
                    // packet timed out, resending
                    UDPC_INTERNAL_PacketInfo newPkt = {
                        cd->addr,
                        0,
                        0,
                        NULL,
                        0,
                        {0, 0}
                    };
                    newPkt.size = pinfo->size - 20;
                    newPkt.data = malloc(newPkt.size);
                    memcpy(newPkt.data, pinfo->data + 20, newPkt.size);
                    free(pinfo->data);

                    pinfo->flags |= 0x4;
                    pinfo->data = NULL;
                    pinfo->size = 0;
                    UDPC_Deque_push_back(
                        cd->priorityPktQueue,
                        &newPkt,
                        sizeof(UDPC_INTERNAL_PacketInfo));
                }
                break;
            }
        }

        --rseq;
    }
}

float UDPC_INTERNAL_ts_diff(struct timespec *ts0, struct timespec *ts1)
{
    float sec = 0.0f;
    if(!ts0 || !ts1)
    {
        return sec;
    }

    if(ts0->tv_sec > ts1->tv_sec)
    {
        sec = ts0->tv_sec - ts1->tv_sec;
        if(ts0->tv_nsec > ts1->tv_nsec)
        {
            sec += ((float)(ts0->tv_nsec - ts1->tv_nsec)) / 1000000000.0f;
        }
        else if(ts0->tv_nsec < ts1->tv_nsec)
        {
            sec -= 1.0f;
            sec += ((float)(1000000000 + ts0->tv_nsec - ts1->tv_nsec)) / 1000000000.0f;
        }
    }
    else if(ts0->tv_sec < ts1->tv_sec)
    {
        sec = ts1->tv_sec - ts0->tv_sec;
        if(ts0->tv_nsec < ts1->tv_nsec)
        {
            sec += ((float)(ts1->tv_nsec - ts0->tv_nsec)) / 1000000000.0f;
        }
        else if(ts0->tv_nsec > ts1->tv_nsec)
        {
            sec -= 1.0f;
            sec += ((float)(1000000000 + ts1->tv_nsec - ts0->tv_nsec)) / 1000000000.0f;
        }
    }
    else
    {
        if(ts0->tv_nsec > ts1->tv_nsec)
        {
            sec += ((float)(ts0->tv_nsec - ts1->tv_nsec)) / 1000000000.0f;
        }
        else
        {
            sec += ((float)(ts1->tv_nsec - ts0->tv_nsec)) / 1000000000.0f;
        }
    }

    return sec;
}

int UDPC_INTERNAL_threadfn(void *context)
{
    UDPC_Context *ctx = (UDPC_Context*)context;

    int shouldStop = 0;
    struct timespec ts;

    while(shouldStop == 0)
    {
        timespec_get(&ts, TIME_UTC);
        ts.tv_nsec += 16666666;
        while(ts.tv_nsec >= 1000000000)
        {
            ts.tv_nsec -= 1000000000;
            ts.tv_sec += 1;
        }
        mtx_lock(&ctx->tCVMtx);
        cnd_timedwait(&ctx->threadCV, &ctx->tCVMtx, &ts);
        UDPC_update(ctx);
        mtx_unlock(&ctx->tCVMtx);

        mtx_lock(&ctx->tflagsMtx);
        shouldStop = ctx->threadFlags & 0x1;
        mtx_unlock(&ctx->tflagsMtx);
    }

    return 0;
}

void UDPC_INTERNAL_prepare_pkt(
    void *data,
    uint32_t protocolID,
    uint32_t conID,
    uint32_t rseq,
    uint32_t ack,
    uint32_t *seqID,
    int flags)
{
    char *d = data;
    uint32_t temp;

    temp = htonl(protocolID);
    memcpy(d, &temp, 4);

    temp = htonl(conID
        | ((flags & 0x1) != 0 ? UDPC_ID_PING : 0)
        | ((flags & 0x2) != 0 ? UDPC_ID_RESENDING : 0)
        | ((flags & 0x4) == 0 ? UDPC_ID_NO_REC_CHK : 0)
        | ((flags & 0x8) != 0 ? UDPC_ID_CONNECT : 0));
    memcpy(&d[4], &temp, 4);

    if(seqID)
    {
        temp = htonl(*seqID);
        ++(*seqID);
    }
    else
    {
        temp = 0;
    }

    memcpy(&d[8], &temp, 4);
    temp = htonl(rseq);
    memcpy(&d[12], &temp, 4);
    temp = htonl(ack);
    memcpy(&d[16], &temp, 4);
}

void UDPC_INTERNAL_log(UDPC_Context *ctx, uint32_t level, const char *msg, ...)
{
    va_list args;
    va_start(args, msg);
    switch(level)
    {
    case 0:
    default:
        if((ctx->flags & 0x4) == 0) break;
        fprintf(stderr, "ERR: ");
        break;
    case 1:
        if((ctx->flags & 0x8) == 0) break;
        fprintf(stderr, "WARN: ");
        break;
    case 2:
        if((ctx->flags & 0x10) == 0) break;
        fprintf(stderr, "INFO: ");
        break;
    case 3:
        if((ctx->flags & 0x20) == 0) break;
        fprintf(stderr, "VERBOSE: ");
        break;
    }
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");
    va_end(args);
}

char* UDPC_INTERNAL_atostr(UDPC_Context *ctx, uint32_t addr)
{
    int index = 0;
    for(int x = 0; x < 4; ++x)
    {
        unsigned char temp = (addr >> (x * 8)) & 0xFF;

        if(temp >= 100)
        {
            ctx->atostrBuf[index++] = '0' + temp / 100;
        }
        if(temp >= 10)
        {
            ctx->atostrBuf[index++] = '0' + ((temp / 10) % 10);
        }
        ctx->atostrBuf[index++] = '0' + temp % 10;

        if(x < 3)
        {
            ctx->atostrBuf[index++] = '.';
        }
    }
    ctx->atostrBuf[index] = 0;

    return ctx->atostrBuf;
}

uint32_t UDPC_INTERNAL_generate_id(UDPC_Context *ctx)
{
    uint32_t newID = 0x10000000;

    while(newID == 0x10000000)
    {
        newID = rand() % 0x10000000;
        if(UDPC_HashMap_has(ctx->idMap, newID) != 0)
        {
            newID = 0x10000000;
        }
    }

    return newID;
}

uint32_t UDPC_strtoa(const char *addrStr)
{
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
