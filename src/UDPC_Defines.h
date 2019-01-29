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

#endif
