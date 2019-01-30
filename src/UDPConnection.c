#include "UDPConnection.h"

#include <stdlib.h>

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
