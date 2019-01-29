#ifndef UDPCONNECTION_H
#define UDPCONNECTION_H

#include <stdio.h>
#include <threads.h>
#include <time.h>
#include <stdint.h>

#include "UDPC_Defines.h"

#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
  #include <winsock2.h>

  #define CleanupSocket(x) closesocket(x)
#elif UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <fcntl.h>
  #include <unistd.h>

  #define CleanupSocket(x) close(x)
#endif

// This struct should not be used outside of this library
typedef struct
{
    uint32_t addr;
    uint32_t id;
    /*
     * 0x1 - is resending
     * 0x2 - is not received checked
     * 0x4 - has been re-sent
     */
    uint32_t flags;
    char *data;
    struct timespec sent;
} UDPC_INTERNAL_PacketInfo;

// This struct should not be used outside of this library
typedef struct
{
    /*
     * 0x1 - trigger send
     * 0x2 - is good mode
     * 0x4 - is good rtt
     */
    uint32_t flags;
    uint32_t id;
    uint32_t lseq;
    uint32_t rseq;
    uint32_t ack;
    float timer;
    float toggleT;
    float toggleTimer;
    float toggledTimer;
    uint16_t port;
    UDPC_INTERNAL_PacketInfo *sentPkts;
    UDPC_INTERNAL_PacketInfo *sendPktQueue;
    struct timespec received;
    struct timespec sent;
    struct timespec rtt;
} UDPC_INTERNAL_ConnectionData;

// This struct should not be modified, only passed to functions that require it
typedef struct
{
    /*
     * 0x1 - is threaded
     */
    uint32_t flags;
    /*
     * 0x1 - thread should stop
     */
    uint32_t threadFlags;
    uint32_t error;
    int socketHandle;
    struct sockaddr_in socketInfo;
    thrd_t threadHandle;
    mtx_t tCVMtx;
    mtx_t tflagsMtx;
    cnd_t threadCV;
} UDPC_Context;

UDPC_Context* UDPC_init(uint16_t listenPort);

UDPC_Context* UDPC_init_threaded_update(uint16_t listenPort);

void UDPC_destroy(UDPC_Context *ctx);

uint32_t UDPC_get_error(UDPC_Context *ctx);

const char* UDPC_get_error_str(uint32_t error);

int UDPC_INTERNAL_threadfn(void *context); // internal usage only

#endif
