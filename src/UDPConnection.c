#include "UDPConnection.h"

#include <stdlib.h>
#include <string.h>

UDPC_Context* UDPC_init(uint16_t listenPort, int isClient)
{
    UDPC_Context *context = malloc(sizeof(UDPC_Context));
    context->error = UDPC_SUCCESS;
    context->flags = 0;
    context->threadFlags = 0;
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

    context->connected = UDPC_Deque_init(sizeof(UDPC_INTERNAL_ConnectionData)
            * (isClient != 0 ? 1 : UDPC_CD_AMOUNT));

    timespec_get(&context->lastUpdated, TIME_UTC);

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
    UDPC_Deque_destroy(ctx->connected);

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

void UDPC_update(UDPC_Context *ctx)
{
    // get dt
    struct timespec tsNow;
    timespec_get(&tsNow, TIME_UTC);
    float dt = UDPC_ts_diff_to_seconds(&tsNow, &ctx->lastUpdated);
    ctx->lastUpdated = tsNow;

    // check timed-out/rtt/send-interval
    UDPC_INTERNAL_ConnectionData *cd;
    UDPC_Deque *removedQueue = UDPC_Deque_init(
        ctx->connected->alloc_size / sizeof(UDPC_INTERNAL_ConnectionData) * sizeof(int));
    for(int x = 0; x * sizeof(UDPC_INTERNAL_ConnectionData) < ctx->connected->size; ++x)
    {
        cd = UDPC_Deque_index_ptr(ctx->connected, sizeof(UDPC_INTERNAL_ConnectionData), x);

        // check if connected timed out
        if(UDPC_ts_diff_to_seconds(&tsNow, &cd->received) >= UDPC_TIMEOUT_SECONDS)
        {
            UDPC_Deque_push_back(removedQueue, &x, sizeof(int));
            // TODO log timed out connection
            continue;
        }

        // check good/bad mode
        cd->toggleTimer += dt;
        cd->toggledTimer += dt;
        if((cd->flags & 0x2) != 0 && (cd->flags & 0x4) == 0)
        {
            // good mode, bad rtt
            // TODO log switching to bad mode
            cd->flags = cd->flags & 0xFFFFFFFD;
            if(cd->toggledTimer >= 10.0f)
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
                // TODO log switching to good mode
                cd->flags |= 0x2;
            }
        }
        else
        {
            // bad mode, bad rtt
            cd->toggledTimer = 0.0f;
        }

        // check send interval
        cd->timer += dt;
        if(cd->timer >= ((cd->flags & 0x2) != 0
            ? UDPC_GOOD_MODE_SEND_INTERVAL : UDPC_BAD_MODE_SEND_INTERVAL))
        {
            cd->timer = 0.0f;
            cd->flags |= 0x1;
        }

    }
    // remove timed out
    for(int x = 0; x * sizeof(int) < removedQueue->size; ++x)
    {
        UDPC_Deque_remove(ctx->connected, sizeof(UDPC_INTERNAL_ConnectionData),
            *((int*)UDPC_Deque_index_rev_ptr(removedQueue, sizeof(int), x)));
    }
    UDPC_Deque_destroy(removedQueue);

    // check triggerSend to send packets to connected
    for(int x = 0; x * sizeof(UDPC_INTERNAL_ConnectionData) < ctx->connected->size; ++x)
    {
        cd = UDPC_Deque_index_ptr(ctx->connected, sizeof(UDPC_INTERNAL_ConnectionData), x);
        if((cd->flags & 0x1) != 0)
        {
            cd->flags &= 0xFFFFFFFE;
            if(cd->sendPktQueue->size == 0)
            {
                // send packet queue is empty, send heartbeat packet
                if(UDPC_ts_diff_to_seconds(&tsNow, &cd->sent) < UDPC_HEARTBEAT_PKT_INTERVAL)
                {
                    continue;
                }

                char *data = malloc(20);
                UDPC_INTERNAL_prepare_pkt(data, cd->id, cd->rseq, cd->ack, &cd->lseq, cd->addr, 0);

                struct sockaddr_in destinationInfo;
                destinationInfo.sin_family = AF_INET;
                destinationInfo.sin_addr.s_addr = htonl(cd->addr);
                destinationInfo.sin_port = htons(cd->port);
                long int sentBytes = sendto(
                    ctx->socketHandle,
                    data,
                    20,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(struct sockaddr_in));
                if(sentBytes != 20)
                {
                    // TODO log fail send packet
                }
                else
                {
                    UDPC_INTERNAL_PacketInfo sentInfo = {
                        cd->addr,
                        cd->lseq - 1,
                        0,
                        NULL,
                        0,
                        tsNow
                    };
                    UDPC_Deque_push_back(
                        cd->sentPkts, &sentInfo, sizeof(UDPC_INTERNAL_PacketInfo));
                    while(cd->sentPkts->size / sizeof(UDPC_INTERNAL_PacketInfo)
                        > UDPC_SENT_PKTS_MAX_SIZE)
                    {
                        UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_get_front_ptr(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo));
                        if(pinfo->data && pinfo->size != 0)
                        {
                            free(pinfo->data);
                        }
                        UDPC_Deque_pop_front(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo));
                    }
                }
                free(data);
            }
            else // sendPktQueue not empty
            {
                UDPC_INTERNAL_PacketInfo *pinfo = UDPC_Deque_get_front_ptr(
                    cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
                char *data = malloc(20 + pinfo->size);
                UDPC_INTERNAL_prepare_pkt(data, cd->id, cd->rseq, cd->ack, &cd->lseq, cd->addr, ((pinfo->flags & 0x3) << 1));
                memcpy(&data[20], pinfo->data, pinfo->size);

                struct sockaddr_in destinationInfo;
                destinationInfo.sin_family = AF_INET;
                destinationInfo.sin_addr.s_addr = htonl(cd->addr);
                destinationInfo.sin_port = htons(cd->port);
                long int sentBytes = sendto(
                    ctx->socketHandle,
                    data,
                    20 + pinfo->size,
                    0,
                    (struct sockaddr*) &destinationInfo,
                    sizeof(struct sockaddr_in));
                if(sentBytes != 20 + pinfo->size)
                {
                    // TODO log fail sent packet
                }
                else
                {
                    if((pinfo->flags & 0x2) != 0)
                    {
                        UDPC_INTERNAL_PacketInfo sentInfo = {
                            cd->addr,
                            cd->lseq - 1,
                            pinfo->flags & 0x2,
                            data,
                            20 + pinfo->size,
                            tsNow
                        };
                        UDPC_Deque_push_back(
                            cd->sentPkts, &sentInfo, sizeof(UDPC_INTERNAL_PacketInfo));
                    }
                    else
                    {
                        UDPC_INTERNAL_PacketInfo sentInfo = {
                            cd->addr,
                            cd->lseq - 1,
                            0,
                            NULL,
                            0,
                            tsNow
                        };
                        UDPC_Deque_push_back(
                            cd->sentPkts, &sentInfo, sizeof(UDPC_INTERNAL_PacketInfo));
                    }

                    while(cd->sentPkts->size / sizeof(UDPC_INTERNAL_PacketInfo)
                        > UDPC_SENT_PKTS_MAX_SIZE)
                    {
                        UDPC_INTERNAL_PacketInfo *pinfoCached = UDPC_Deque_get_front_ptr(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo));
                        if(pinfoCached->data && pinfoCached->size != 0)
                        {
                            free(pinfoCached->data);
                        }
                        UDPC_Deque_pop_front(cd->sentPkts, sizeof(UDPC_INTERNAL_PacketInfo));
                    }
                }
                free(pinfo->data);
                UDPC_Deque_pop_front(cd->sendPktQueue, sizeof(UDPC_INTERNAL_PacketInfo));
            }
        }
    }

    // receive packet
    // TODO
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
    uint32_t addr,
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
