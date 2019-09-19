#ifndef UDPC_CONNECTION_H
#define UDPC_CONNECTION_H

// Determine platform macros
#define UDPC_PLATFORM_WINDOWS 1
#define UDPC_PLATFORM_MAC 2
#define UDPC_PLATFORM_LINUX 3
#define UDPC_PLATFORM_UNKNOWN 0

#if defined _WIN32
#define UDPC_PLATFORM UDPC_PLATFORM_WINDOWS
#elif defined __APPLE__
#define UDPC_PLATFORM UDPC_PLATFORM_MAC
#elif defined __linux__
#define UDPC_PLATFORM UDPC_PLATFORM_LINUX
#else
#define UDPC_PLATFORM UDPC_PLATFORM_UNKNOWN
#endif

// OS-based networking macros
#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
#include <winsock2.h>
#include <Ws2ipdef.h>
#include <In6addr.h>

#define UDPC_CLEANUPSOCKET(x) closesocket(x)
#define UDPC_SOCKETTYPE SOCKET
#define UDPC_IPV6_SOCKADDR_TYPE SOCKADDR_IN6
#define UDPC_IPV6_ADDR_TYPE IN6_ADDR
#define UDPC_IPV6_ADDR_SUB(addr) addr.u.Byte
#define UDPC_SOCKET_RETURN_ERROR(socket) (socket == INVALID_SOCKET)
#elif UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define UDPC_CLEANUPSOCKET(x) close(x)
#define UDPC_SOCKETTYPE int
#define UDPC_IPV6_SOCKADDR_TYPE struct sockaddr_in6
#define UDPC_IPV6_ADDR_TYPE struct in6_addr
#define UDPC_IPV6_ADDR_SUB(addr) addr.s6_addr
#define UDPC_SOCKET_RETURN_ERROR(socket) (socket <= 0)
#else
#define UDPC_CLEANUPSOCKET(x) ((void)0)
#endif

// other defines
#define UDPC_PACKET_MAX_SIZE 8192
#define UDPC_DEFAULT_PROTOCOL_ID 1357924680 // 0x50f04948

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

// Opaque struct handle to Context
struct UDPC_Context;
typedef struct UDPC_Context *UDPC_HContext;

typedef enum { UDPC_SILENT, UDPC_ERROR, UDPC_WARNING, UDPC_VERBOSE, UDPC_INFO } UDPC_LoggingType;

typedef struct {
    UDPC_IPV6_ADDR_TYPE addr;
    uint32_t scope_id;
    uint16_t port;
} UDPC_ConnectionId;

typedef struct {
    // id is stored at offset 8, size 4 (uint32_t) even for "empty" PktInfos
    char data[UDPC_PACKET_MAX_SIZE];
    /*
     * 0x1 - connect
     * 0x2 - ping
     * 0x4 - no_rec_chk
     * 0x8 - resending
     */
    uint32_t flags;
    uint16_t dataSize; // zero if invalid
    UDPC_ConnectionId sender;
    UDPC_ConnectionId receiver;
} UDPC_PacketInfo;

/// port should be in native byte order (not network/big-endian)
UDPC_ConnectionId UDPC_create_id(UDPC_IPV6_ADDR_TYPE addr, uint16_t port);

UDPC_ConnectionId UDPC_create_id_full(UDPC_IPV6_ADDR_TYPE addr, uint32_t scope_id, uint16_t port);

UDPC_ConnectionId UDPC_create_id_anyaddr(uint16_t port);

UDPC_HContext UDPC_init(UDPC_ConnectionId listenId, int isClient);
UDPC_HContext UDPC_init_threaded_update(UDPC_ConnectionId listenId,
                                int isClient);

void UDPC_destroy(UDPC_HContext ctx);

void UDPC_update(UDPC_HContext ctx);

void UDPC_client_initiate_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId);

int UDPC_get_queue_send_available(UDPC_HContext ctx, UDPC_ConnectionId connectionId);

void UDPC_queue_send(UDPC_HContext ctx, UDPC_ConnectionId destinationId,
                     uint32_t isChecked, void *data, uint32_t size);

int UDPC_set_accept_new_connections(UDPC_HContext ctx, int isAccepting);

int UDPC_drop_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId, bool dropAllWithAddr);

int UDPC_has_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId);

uint32_t UDPC_set_protocol_id(UDPC_HContext ctx, uint32_t id);

UDPC_LoggingType UDPC_set_logging_type(UDPC_HContext ctx, UDPC_LoggingType loggingType);

UDPC_PacketInfo UDPC_get_received(UDPC_HContext ctx);

const char *UDPC_atostr_cid(UDPC_HContext ctx, UDPC_ConnectionId connectionId);

const char *UDPC_atostr(UDPC_HContext ctx, UDPC_IPV6_ADDR_TYPE addr);

/// addrStr must be a valid ipv6 address or a valid ipv4 address
UDPC_IPV6_ADDR_TYPE UDPC_strtoa(const char *addrStr);

UDPC_IPV6_ADDR_TYPE UDPC_strtoa_link(const char *addrStr, uint32_t *linkId_out);

#ifdef __cplusplus
}
#endif
#endif
