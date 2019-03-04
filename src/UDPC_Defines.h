#ifndef UDPC_DEFINES_H
#define UDPC_DEFINES_H

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

#define UDPC_SUCCESS 0
#define UDPC_ERR_SOCKETFAIL 1 // failed to create socket
#define UDPC_ERR_SOCKETBINDF 2 // failed to bind socket
#define UDPC_ERR_SOCKETNONBF 3 // failed to set non-blocking on socket
#define UDPC_ERR_MTXFAIL 4 // failed to create mutex
#define UDPC_ERR_CVFAIL 5 // failed to create condition variable
#define UDPC_ERR_THREADFAIL 6 // failed to create thread

static const char *UDPC_ERR_SOCKETFAIL_STR = "Failed to create socket";
static const char *UDPC_ERR_SOCKETBINDF_STR = "Failed to bind socket";
static const char *UDPC_ERR_SOCKETNONBF_STR = "Failed to set non-blocking on socket";
static const char *UDPC_ERR_MTXFAIL_STR = "Failed to create mutex";
static const char *UDPC_ERR_CVFAIL_STR = "Failed to create condition variable";
static const char *UDPC_ERR_THREADFAIL_STR = "Failed to create thread";

#define UDPC_CD_AMOUNT 32

#define UDPC_GOOD_MODE_SEND_INTERVAL (1.0f/30.0f)
#define UDPC_BAD_MODE_SEND_INTERVAL (1.0f/10.0f)
#define UDPC_TIMEOUT_SECONDS 10.0f
#define UDPC_HEARTBEAT_PKT_INTERVAL (15.0f/100.0f)
#define UDPC_INIT_PKT_INTERVAL 5
#define UDPC_INIT_PKT_INTERVAL_F ((float)UDPC_INIT_PKT_INTERVAL)
#define UDPC_PKT_PROTOCOL_ID 1357924680

#define UDPC_ID_CONNECT        0x80000000
#define UDPC_ID_PING           0x40000000
#define UDPC_ID_NO_REC_CHK     0x20000000
#define UDPC_ID_RESENDING      0x10000000

#define UDPC_SENT_PKTS_MAX_SIZE 34
#define UDPC_SENT_PKTS_ALLOC_SIZE 35
#define UDPC_SEND_PKTS_ALLOC_SIZE 40
#define UDPC_RESEND_PKTS_ALLOC_SIZE 40

#define UDPC_PACKET_MAX_SIZE 8192

#define UDPC_PACKET_TIMEOUT_SEC 1.0f
#define UDPC_GOOD_RTT_LIMIT_SEC 0.25f

#define UDPC_REC_PKTS_ALLOC_SIZE 128

#define UDPC_CONNECTED_EVENT_SIZE 64
#define UDPC_DISCONNECTED_EVENT_SIZE 64

#endif
