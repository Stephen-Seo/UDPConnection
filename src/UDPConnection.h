#ifndef UDPCONNECTION_H
#define UDPCONNECTION_H

#include <stdio.h>
#include <threads.h>

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

#define UDPCON_ERR_SOCKETFAIL 1 // failed to create socket
#define UDPCON_ERR_SOCKETBINDF 2 // failed to bind socket
#define UDPCON_ERR_SOCKETNONBF 3 // failed to set non-blocking on socket
#define UDPCON_ERR_MTXFAIL 4 // failed to create mutex
#define UDPCON_ERR_CVFAIL 5 // failed to create condition variable
#define UDPCON_ERR_THREADFAIL 6 // failed to create thread

// This struct should not be modified, only passed to functions that require it
typedef struct
{
    int error;
    int socketHandle;
    struct sockaddr_in socketInfo;
    thrd_t threadHandle;
    mtx_t tCVMtx;
    mtx_t tflagsMtx;
    cnd_t threadCV;
    /*
     * 0x1 - is threaded
     */
    int flags;
    /*
     * 0x1 - thread should stop
     */
    int threadFlags;
} UDPC_Context;

UDPC_Context UDPC_init(unsigned short listenPort);

UDPC_Context UDPC_init_threaded_update(unsigned short listenPort);

void UDPC_destroy(UDPC_Context *ctx);

int UDPC_INTERNAL_threadfn(void *context); // internal usage only

#endif
