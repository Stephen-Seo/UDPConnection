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

#define CleanupSocket(x) closesocket(x)
#elif UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define CleanupSocket(x) close(x)
#else
#define CleanupSocket(x) ((void)0)
#endif

// other defines
#define UDPC_PACKET_MAX_SIZE 8192
#define UDPC_DEFAULT_PROTOCOL_ID 1357924680

#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

typedef enum { SILENT, ERROR, WARNING, VERBOSE, INFO } UDPC_LoggingType;

typedef struct {
    char data[UDPC_PACKET_MAX_SIZE];
    uint16_t dataSize; // zero if invalid
    uint32_t sender;
    uint32_t receiver;
    uint16_t senderPort;
    uint16_t receiverPort;
} PacketInfo;

void *UDPC_init(uint16_t listenPort, uint32_t listenAddr, int isClient);
void *UDPC_init_threaded_update(uint16_t listenPort, uint32_t listenAddr,
                                int isClient);

void UDPC_destroy(void *ctx);

void UDPC_update(void *ctx);

int UDPC_get_queue_send_available(void *ctx, uint32_t addr);

void UDPC_queue_send(void *ctx, uint32_t destAddr, uint16_t destPort,
                     uint32_t isChecked, void *data, uint32_t size);

int UDPC_set_accept_new_connections(void *ctx, int isAccepting);

int UDPC_drop_connection(void *ctx, uint32_t addr, uint16_t port);

uint32_t UDPC_set_protocol_id(void *ctx, uint32_t id);

UDPC_LoggingType set_logging_type(void *ctx, UDPC_LoggingType loggingType);

PacketInfo UDPC_get_received(void *ctx);

const char *UDPC_atostr(void *ctx, uint32_t addr);

uint32_t UDPC_strtoa(const char *addrStr);

#ifdef __cplusplus
}
#endif
#endif
