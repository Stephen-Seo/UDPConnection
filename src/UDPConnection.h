#ifndef UDPCONNECTION_H
#define UDPCONNECTION_H

#include <stdio.h>
#include <threads.h>
#include <time.h>
#include <stdint.h>

#include "UDPC_Defines.h"
#include "UDPC_Deque.h"
#include "UDPC_HashMap.h"

#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
  #include <winsock2.h>

  #define CleanupSocket(x) closesocket(x)
#elif UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <fcntl.h>
  #include <unistd.h>

  #define CleanupSocket(x) close(x)
#else
  #define CleanupSocket(x) ((void)0)
#endif

#define UDPC_ATOSTR_BUF_SIZE 16

/// This struct should not be used outside of this library
typedef struct {
    uint32_t addr; // in network order (big-endian)
    uint32_t id;
    /*
     * 0x1 - is resending
     * 0x2 - is check received packet
     * 0x4 - has been re-sent
     */
    uint32_t flags;
    char *data; // no-header in sendPktQueue, header in sentPkts
    uint32_t size;
    struct timespec sent;
} UDPC_INTERNAL_PacketInfo;

/// This struct should not be used outside of this library
typedef struct {
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
    uint32_t addr; // in network order (big-endian)
    uint16_t port;
    UDPC_Deque *sentPkts;
    UDPC_Deque *sendPktQueue;
    struct timespec received;
    struct timespec sent;
    float rtt;
} UDPC_INTERNAL_ConnectionData;

/// This struct should not be modified, only passed to functions that require it
typedef struct {
    /*
     * 0x1 - is threaded
     * 0x2 - is client
     * 0x4 - log errors
     * 0x8 - log warnings
     * 0x10 - log info
     * 0x20 - log verbose
     * 0x40 - accept new connections
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
    UDPC_HashMap *conMap;
    struct timespec lastUpdated;
    char atostrBuf[UDPC_ATOSTR_BUF_SIZE];
    char recvBuf[UDPC_PACKET_MAX_SIZE];
} UDPC_Context;

typedef struct {
    struct timespec tsNow;
    float dt;
    UDPC_Deque *removedQueue;
    UDPC_Context *ctx;
} UDPC_INTERNAL_update_struct;

UDPC_Context* UDPC_init(uint16_t listenPort, int isClient);

UDPC_Context* UDPC_init_threaded_update(uint16_t listenPort, int isClient);

void UDPC_destroy(UDPC_Context *ctx);

void UDPC_INTERNAL_destroy_conMap(void *unused, uint32_t addr, char *data);

uint32_t UDPC_get_error(UDPC_Context *ctx);

const char* UDPC_get_error_str(uint32_t error);

/*!
 * 0 - log nothing
 * 1 - log only errors
 * 2 - log only errors and warnings
 * 3 - log errors, warnings, and info
 * 4+ - log everything
 *
 * By default, erros and warnings are logged.
 */
void UDPC_set_logging_type(UDPC_Context *ctx, uint32_t logType);

/// If threaded, this function is called automatically
void UDPC_update(UDPC_Context *ctx);

void UDPC_INTERNAL_update_to_rtt_si(void *userData, uint32_t addr, char *data);

void UDPC_INTERNAL_update_send(void *userData, uint32_t addr, char *data);

void UDPC_INTERNAL_update_rtt(
    UDPC_Context *ctx,
    UDPC_INTERNAL_ConnectionData *cd,
    uint32_t rseq,
    struct timespec *tsNow);

void UDPC_INTERNAL_check_pkt_timeout(
    UDPC_INTERNAL_ConnectionData *cd,
    uint32_t rseq,
    uint32_t ack,
    struct timespec *tsNow);

float UDPC_ts_diff_to_seconds(struct timespec *ts0, struct timespec *ts1);

int UDPC_INTERNAL_threadfn(void *context); // internal usage only

/*
 * 0x1 - is ping
 * 0x2 - is resending
 * 0x4 - is checked received packet
 */
void UDPC_INTERNAL_prepare_pkt(
    void *data,
    uint32_t conID,
    uint32_t rseq,
    uint32_t ack,
    uint32_t *seqID,
    int flags);

/*!
 * 0 - error
 * 1 - warning
 * 2 - info
 * 3 - verbose
 */
void UDPC_INTERNAL_log(UDPC_Context *ctx, uint32_t level, const char *msg, ...);

char* UDPC_INTERNAL_atostr(UDPC_Context *ctx, uint32_t addr);

#endif
