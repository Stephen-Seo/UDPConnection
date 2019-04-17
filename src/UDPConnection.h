#ifndef UDPCONNECTION_H
#define UDPCONNECTION_H

/*!
 * Any function or struct starting with "UDPC_INTERNAL" should never be used,
 * as they are used internally by this library.
 */

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

/// (void *userData, uint32_t address)
/*!
 * Note address is in network byte order (usually big-endian)
 */
typedef void (*UDPC_callback_connected)(void*, uint32_t);

/// (void *userData, uint32_t address)
/*!
 * Note address is in network byte order (usually big-endian)
 */
typedef void (*UDPC_callback_disconnected)(void*, uint32_t);

/// (void *userData, uint32_t address, char *packetData, uint32_t packetSize)
/*!
 * The data pointed to by the packetData argument is to data internally managed
 * by the UDPC_Context. It will change every time this callback is called so do
 * not depend on it persisting. This means you should copy the data out of it
 * when the callback is invoked and work with the copied data.
 */
typedef void (*UDPC_callback_received)(void*, uint32_t, char*, uint32_t);

/// This struct should not be used outside of this library
typedef struct {
    uint32_t addr; // in network order (big-endian)
    uint32_t id;
    /*
     * 0x1 - is resending
     * 0x2 - is check received packet
     * 0x4 - has been re-sent
     * 0x8 - initiate connection packet
     */
    uint32_t flags;
    char *data; // no-header in sendPktQueue and receivedPackets, header in sentPkts
    uint32_t size;
    struct timespec sent;
} UDPC_INTERNAL_PacketInfo;

/// This struct should not be used outside of this library
typedef struct {
    /*
     * 0x1 - trigger send
     * 0x2 - is good mode
     * 0x4 - is good rtt
     * 0x8 - initiating connection to server
     * 0x10 - is id set
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
    UDPC_Deque *priorityPktQueue;
    struct timespec received;
    struct timespec sent;
    float rtt;
} UDPC_INTERNAL_ConnectionData;

/// This struct should not be used externally, only passed to functions that require it
typedef struct {
    int isThreaded;
    /*
     * 0x1 - unused
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
    uint32_t protocolID;
    uint32_t error;
    int socketHandle;
    struct sockaddr_in socketInfo;
    thrd_t threadHandle;
    mtx_t tCVMtx;
    mtx_t tflagsMtx;
    cnd_t threadCV;
    UDPC_HashMap *conMap;
    // Clients intentionally do not use idMap at all
    UDPC_HashMap *idMap;
    struct timespec lastUpdated;
    char atostrBuf[UDPC_ATOSTR_BUF_SIZE];
    char recvBuf[UDPC_PACKET_MAX_SIZE];
    UDPC_Deque *connectedEvents;
    UDPC_Deque *disconnectedEvents;
    UDPC_Deque *receivedPackets;

    UDPC_callback_connected callbackConnected;
    void *callbackConnectedUserData;
    UDPC_callback_disconnected callbackDisconnected;
    void *callbackDisconnectedUserData;
    UDPC_callback_received callbackReceived;
    void *callbackReceivedUserData;
} UDPC_Context;

/// This struct should not be used outside of this library
typedef struct {
    struct timespec tsNow;
    float dt;
    UDPC_Deque *removedQueue;
    UDPC_Context *ctx;
} UDPC_INTERNAL_update_struct;

/// Creates a new UDPC_Context, for establishing a connection via UDP
/*!
 * Callbacks must be set to use UDPC effectively, using the following functions:
 * - UDPC_set_callback_connected()
 * - UDPC_set_callback_disconnected()
 * - UDPC_set_callback_received()
 *
 * Clients should also use UDPC_client_initiate_connection() to let UDPC know
 * what server to connect to.
 *
 * UDPC_update() must be called periodically (ideally at 30 updates per second
 * or faster), to send and receive UDP packets, and establish new connections
 * when enabled. An init variant "UDPC_init_threaded_update()" is available
 * which will call update periodically on a separate thread.
 *
 * UDPC_check_events() must also be called to call the set callbacks for
 * connected, disconnected, and received events.
 *
 * When finished using UDPC, UDPC_destroy must be called on the context to free
 * resources.
 */
UDPC_Context* UDPC_init(uint16_t listenPort, uint32_t listenAddr, int isClient);

/// Creates a new UDPC_Context, where a thread is created to update UDPC on its own
/*!
 * If using a UDPC_Context created by this init variant, UDPC_update() must not
 * be manually called, as it is called automatically by a separate thread.
 *
 * See the documentation for UDPC_init() for more details.
 */
UDPC_Context* UDPC_init_threaded_update(uint16_t listenPort, uint32_t listenAddr, int isClient);

/// This fn must be called on a UDPC_Context to free resources
void UDPC_destroy(UDPC_Context *ctx);

void UDPC_INTERNAL_destroy_conMap(void *unused, uint32_t addr, char *data);

/// Sets the callback for connected events
void UDPC_set_callback_connected(
    UDPC_Context *ctx, UDPC_callback_connected fptr, void *userData);

/// Sets the callback for disconnected events
void UDPC_set_callback_disconnected(
    UDPC_Context *ctx, UDPC_callback_disconnected fptr, void *userData);

/// Sets the callback for received packet events
void UDPC_set_callback_received(
    UDPC_Context *ctx, UDPC_callback_received fptr, void *userData);

/// Invokes callbacks based on events that have ocurred during UDPC_update()
void UDPC_check_events(UDPC_Context *ctx);

/// Tells UDPC to initiate a connection to a server
void UDPC_client_initiate_connection(UDPC_Context *ctx, uint32_t addr, uint16_t port);

/*!
 * \brief Queues a packet to send to a connected peer
 * Note addr is expected to be in network-byte-order (big-endian).
 * If isChecked is non-zero, UDPC will attempt to resend the packet if peer has
 * not received it within UDPC_PACKET_TIMEOUT_SEC seconds.
 * \return non-zero on success
 */
int UDPC_queue_send(
    UDPC_Context *ctx, uint32_t addr, uint32_t isChecked, void *data, uint32_t size);

/*!
 * \brief get the number of packets that can be queued to the addr
 * \return number of queueable packets or 0 if connection has not been established
 */
int UDPC_get_queue_send_available(UDPC_Context *ctx, uint32_t addr);

/// Returns non-zero if UDPC is accepting new connections
int UDPC_get_accept_new_connections(UDPC_Context *ctx);

/// Set isAccepting to non-zero to let UDPC accept new connections
/*!
 * Set isAccepting to zero to prevent UDPC from accepting new connections.
 */
void UDPC_set_accept_new_connections(UDPC_Context *ctx, int isAccepting);

/// Drops a connection specified by addr
/*!
 * \return non-zero if the connection existed and was dropped
 */
int UDPC_drop_connection(UDPC_Context *ctx, uint32_t addr);

/// Gets the currently set protocol id
uint32_t UDPC_get_protocol_id(UDPC_Context *ctx);

/// Sets the protocol id
/*!
 * Note that UDPC can only connect to other UDPC instances that use the same
 * protocol id.
 */
void UDPC_set_protocol_id(UDPC_Context *ctx, uint32_t id);

/*!
 * \brief Get the currently set error code, and clear it internally
 * Error codes and their meanings are defined in UDPC_Defines.h .
 * Use UDPC_get_error_str() to get a string describing the error.
 */
uint32_t UDPC_get_error(UDPC_Context *ctx);

/// Returns a string describing the error code for UDPC
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
    UDPC_Context *ctx,
    UDPC_INTERNAL_ConnectionData *cd,
    uint32_t rseq,
    uint32_t ack,
    struct timespec *tsNow);

float UDPC_INTERNAL_ts_diff(struct timespec *ts0, struct timespec *ts1);

int UDPC_INTERNAL_threadfn(void *context);

/*
 * 0x1 - is ping
 * 0x2 - is resending
 * 0x4 - is checked received packet
 * 0x8 - is init connection packet
 */
void UDPC_INTERNAL_prepare_pkt(
    void *data,
    uint32_t protocolID,
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

uint32_t UDPC_INTERNAL_generate_id(UDPC_Context *ctx);

/*1
 * \brief Converts a IPV4 string to a 32-bit unsigned integer address in big-endian
 * \return 0 if string is invalid, address in big-endian format otherwise
 */
uint32_t UDPC_strtoa(const char *addrStr);

#endif
