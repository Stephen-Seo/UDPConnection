#include "UDPConnection.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

UDPC_Context* UDPC_init(uint16_t listenPort, int isClient)
{
    UDPC_Context *context = malloc(sizeof(UDPC_Context));
    context->error = UDPC_SUCCESS;
    context->flags = 0;
    context->threadFlags = 0;
    context->atostrBuf[UDPC_ATOSTR_BUF_SIZE - 1] = 0;
    if(isClient != 0) context->flags |= 0x2;

    // create socket
    context->socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(context->socketHandle <= 0)
    {
        context->socketHandle = 0;
        context->error = UDPC_ERR_SOCKETFAIL;
        fprintf(stderr, "Failed to create socket\n");
        return context;
    }

    // bind socket
    context->socketInfo.sin_family = AF_INET;
    context->socketInfo.sin_addr.s_addr = INADDR_ANY;
    context->socketInfo.sin_port = listenPort;
    if(bind(
            context->socketHandle,
            (const struct sockaddr*) &context->socketInfo,
            sizeof(struct sockaddr_in)
        ) < 0)
    {
        context->error = UDPC_ERR_SOCKETBINDF;
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        fprintf(stderr, "Failed to bind socket\n");
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
        fprintf(stderr, "Failed to set non-blocking on socket\n");
#if UDPC_PLATFORM == UDPC_PLATFORM_UNKNOWN
        fprintf(stderr, "(Unknown platform)\n");
#endif
        return context;
    }

    context->conMap = UDPC_HashMap_init(13, sizeof(UDPC_INTERNAL_ConnectionData));

    timespec_get(&context->lastUpdated, TIME_UTC);

    context->flags |= (0x8 | 0x4);

    return context;
}

UDPC_Context* UDPC_init_threaded_update(uint16_t listenPort, int isClient)
{
    UDPC_Context *context = UDPC_init(listenPort, isClient);

    context->error = mtx_init(&context->tCVMtx, mtx_timed);
    if(context->error != thrd_success)
    {
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        fprintf(stderr, "Failed to create mutex\n");
        context->error = UDPC_ERR_MTXFAIL;
    }
    context->error = UDPC_SUCCESS;

    context->error = mtx_init(&context->tflagsMtx, mtx_timed);
    if(context->error != thrd_success)
    {
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        mtx_destroy(&context->tCVMtx);
        fprintf(stderr, "Failed to create mutex\n");
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
        fprintf(stderr, "Failed to create condition variable\n");
        context->error = UDPC_ERR_CVFAIL;
        return context;
    }
    context->error = UDPC_SUCCESS;

    context->error = thrd_create(
        &context->threadHandle, UDPC_INTERNAL_threadfn, &context);
    if(context->error != thrd_success)
    {
        CleanupSocket(context->socketHandle);
        context->socketHandle = 0;
        mtx_destroy(&context->tCVMtx);
        mtx_destroy(&context->tflagsMtx);
        cnd_destroy(&context->threadCV);
        fprintf(stderr, "Failed to create thread\n");
        context->error = UDPC_ERR_THREADFAIL;
        return context;
    }
    context->error = UDPC_SUCCESS;

    return context;
}

void UDPC_destroy(UDPC_Context *ctx)
{
    CleanupSocket(ctx->socketHandle);
    UDPC_HashMap_itercall(ctx->conMap, UDPC_INTERNAL_destroy_conMap, NULL);
    UDPC_HashMap_destroy(ctx->conMap);

    if((ctx->flags & 0x1) != 0)
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
}

uint32_t UDPC_get_error(UDPC_Context *ctx)
{
    uint32_t error = ctx->error;
    ctx->error = 0;
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
    us.dt = UDPC_ts_diff_to_seconds(&us.tsNow, &ctx->lastUpdated);
    ctx->lastUpdated = us.tsNow;
    us.removedQueue = UDPC_Deque_init(4 * (ctx->conMap->size));

    UDPC_HashMap_itercall(ctx->conMap, UDPC_INTERNAL_update_to_rtt_si, &us);

    // remove timed out
    for(int x = 0; x * 4 < us.removedQueue->size; ++x)
    {
        uint32_t *key = UDPC_Deque_index_ptr(us.removedQueue, 4, x);
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
    if(bytes < 20)
    {
        UDPC_INTERNAL_log(ctx, 2, "Got invalid packet from %s port %d (too small)",
            UDPC_INTERNAL_atostr(ctx, receivedData.sin_addr.s_addr),
            ntohs(receivedData.sin_port));
        return;
    }

    uint32_t temp = ntohl(*((uint32_t*)ctx->recvBuf));
    if(temp != UDPC_PKT_PROTOCOL_ID)
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
        if(!UDPC_HashMap_get(ctx->conMap, receivedData.sin_addr.s_addr))
        {
            UDPC_INTERNAL_log(ctx, 2, "Establishing connection with %s port %d",
                UDPC_INTERNAL_atostr(ctx, receivedData.sin_addr.s_addr),
                ntohs(receivedData.sin_port));
            UDPC_INTERNAL_ConnectionData newCD = {
                1,
                0,
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
                UDPC_Deque_init(sizeof(UDPC_INTERNAL_PacketInfo) * UDPC_SENT_PKTS_ALLOC_SIZE),
                {0, 0},
                {0, 0},
                {1, 0}
            };
            timespec_get(&newCD.received, TIME_UTC);
            timespec_get(&newCD.sent, TIME_UTC);
            UDPC_HashMap_insert(ctx->conMap, newCD.addr, &newCD);
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

    int isOutOfOrder = 0;
    // TODO rest of received packet actions
}

void UDPC_INTERNAL_update_to_rtt_si(void *userData, uint32_t addr, char *data)
{
    UDPC_INTERNAL_update_struct *us =
        (UDPC_INTERNAL_update_struct*)userData;
    UDPC_INTERNAL_ConnectionData *cd = (UDPC_INTERNAL_ConnectionData*)data;

    // check for timed out connection
    if(UDPC_ts_diff_to_seconds(&us->tsNow, &cd->received) >= UDPC_TIMEOUT_SECONDS)
    {
        UDPC_Deque_push_back(us->removedQueue, &addr, 4);
        UDPC_INTERNAL_log(us->ctx, 2, "Connection timed out with addr %s port %d",
            UDPC_INTERNAL_atostr(us->ctx, addr),
            cd->port);
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
    if(cd->sendPktQueue->size == 0)
    {
        // send packet queue is empty, send heartbeat packet
        if(UDPC_ts_diff_to_seconds(&us->tsNow, &cd->sent) < UDPC_HEARTBEAT_PKT_INTERVAL)
        {
            return;
        }

        char *data = malloc(20);
        UDPC_INTERNAL_prepare_pkt(data, cd->id, cd->rseq, cd->ack, &cd->lseq, 0);

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
    else // sendPktQueue not empty
    {
        UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_get_front_ptr(
            cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
        char *data = malloc(20 + pinfo->size);
        UDPC_INTERNAL_prepare_pkt(
            data,
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
            UDPC_Deque_pop_front(cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
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
        UDPC_Deque_pop_front(cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
    }
}

float UDPC_ts_diff_to_seconds(struct timespec *ts0, struct timespec *ts1)
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
        mtx_unlock(&ctx->tCVMtx);

        mtx_lock(&ctx->tflagsMtx);
        shouldStop = ctx->threadFlags & 0x1;
        mtx_unlock(&ctx->tflagsMtx);
    }

    return 0;
}

void UDPC_INTERNAL_prepare_pkt(
    void *data,
    uint32_t conID,
    uint32_t rseq,
    uint32_t ack,
    uint32_t *seqID,
    int flags)
{
    char *d = data;
    uint32_t temp;

    temp = htonl(UDPC_PKT_PROTOCOL_ID);
    memcpy(d, &temp, 4);
    if((flags & 0x4) == 0)
    {
        temp = htonl(conID | UDPC_ID_NO_REC_CHK);
        memcpy(&d[4], &temp, 4);
    }
    else if((flags & 0x1) != 0)
    {
        temp = htonl(conID | UDPC_ID_PING);
        memcpy(&d[4], &temp, 4);
    }
    else
    {
        temp = htonl(conID | ((flags & 0x2) != 0 ? UDPC_ID_RESENDING : 0));
        memcpy(&d[4], &temp, 4);
    }
    temp = htonl(*seqID);
    ++(*seqID);
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
        unsigned char temp = (addr >> (24 - x * 8)) & 0xFF;

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
