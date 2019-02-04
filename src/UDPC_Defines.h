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
#define UDPC_PKT_PROTOCOL_ID 1357924680

#define UDPC_ID_CONNECT        0x80000000
#define UDPC_ID_PING           0x40000000
#define UDPC_ID_NO_REC_CHK     0x20000000
#define UDPC_ID_RESENDING      0x10000000

#define UDPC_SENT_PKTS_MAX_SIZE 34
#define UDPC_SENT_PKTS_ALLOC_SIZE 35

#define UDPC_PACKET_MAX_SIZE 8192

// 5 8 2 7 3 6 1
// 3 2 5 1 8 7 6
#define UDPC_HASH32(x) ( \
    ( \
      ((x & 0xF8000000) >> 5) \
      ((x & 0x07F80000) >> 6) \
      ((x & 0x00060000) << 10) \
      ((x & 0x0001FC00) >> 4) \
      ((x & 0x00000380) << 22) \
      ((x & 0x0000007E) >> 1) \
      ((x & 0x00000001) << 21) \
    ) ^ 0x96969696 \
)

#endif
