/*!
 * \mainpage UDPConnection
 * \ref UDPConnection.h
 */

/*!
 * \file UDPConnection.h
 * \brief Public API for UDPConnection
 */

#ifndef UDPC_CONNECTION_H
#define UDPC_CONNECTION_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// Determine platform macros
# define UDPC_PLATFORM_WINDOWS 1
# define UDPC_PLATFORM_MAC 2
# define UDPC_PLATFORM_LINUX 3
# define UDPC_PLATFORM_UNKNOWN 0

# if defined _WIN32
#  define UDPC_PLATFORM UDPC_PLATFORM_WINDOWS
# elif defined __APPLE__
#  define UDPC_PLATFORM UDPC_PLATFORM_MAC
# elif defined __linux__
#  define UDPC_PLATFORM UDPC_PLATFORM_LINUX
# else
#  define UDPC_PLATFORM UDPC_PLATFORM_UNKNOWN
# endif

#endif // DOXYGEN_SHOULD_SKIP_THIS

// OS-based networking macros
#if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
# include <winsock2.h>
# ifdef UDPC_PLATFORM_MINGW
#  include <ws2ipdef.h>
#  include <in6addr.h>
# else
#  include <Ws2ipdef.h>
#  include <In6addr.h>
# endif

# ifndef DOXYGEN_SHOULD_SKIP_THIS

#  define UDPC_CLEANUPSOCKET(x) closesocket(x)
#  define UDPC_SOCKETTYPE SOCKET
#  define UDPC_IPV6_SOCKADDR_TYPE SOCKADDR_IN6
#  define UDPC_IPV6_ADDR_TYPE IN6_ADDR
#  define UDPC_IPV6_ADDR_SUB(addr) addr.u.Byte
#  define UDPC_SOCKET_RETURN_ERROR(socket) (socket == INVALID_SOCKET)

# endif // DOXYGEN_SHOULD_SKIP_THIS

#elif UDPC_PLATFORM == UDPC_PLATFORM_MAC || UDPC_PLATFORM == UDPC_PLATFORM_LINUX
# include <fcntl.h>
# include <netinet/in.h>
# include <sys/socket.h>
# include <unistd.h>

# ifndef DOXYGEN_SHOULD_SKIP_THIS

#  define UDPC_CLEANUPSOCKET(x) close(x)
#  define UDPC_SOCKETTYPE int
#  define UDPC_IPV6_SOCKADDR_TYPE struct sockaddr_in6
#  define UDPC_IPV6_ADDR_TYPE struct in6_addr
#  define UDPC_IPV6_ADDR_SUB(addr) addr.s6_addr
#  define UDPC_SOCKET_RETURN_ERROR(socket) (socket <= 0)

# endif // DOXYGEN_SHOULD_SKIP_THIS

#else
# ifndef DOXYGEN_SHOULD_SKIP_THIS
#  define UDPC_CLEANUPSOCKET(x) ((void)0)
# endif // DOXYGEN_SHOULD_SKIP_THIS
#endif

// other defines
#define UDPC_PACKET_MAX_SIZE 8192
#define UDPC_DEFAULT_PROTOCOL_ID 1357924680 // 0x50f04948

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// other defines continued

# ifndef UDPC_LIBSODIUM_ENABLED
#  ifndef crypto_sign_PUBLICKEYBYTES
#   define crypto_sign_PUBLICKEYBYTES 1
#  endif
#  ifndef crypto_sign_SECRETKEYBYTES
#   define crypto_sign_SECRETKEYBYTES 1
#  endif
#  ifndef crypto_sign_BYTES
#   define crypto_sign_BYTES 1
#  endif
# endif

#endif // DOXYGEN_SHOULD_SKIP_THIS

#ifdef __cplusplus
# include <cstdint>
extern "C" {
#else
# include <stdint.h>
#endif

/// Opaque struct handle to Context
struct UDPC_Context;
typedef struct UDPC_Context *UDPC_HContext;

typedef enum {
    UDPC_SILENT,
    UDPC_ERROR,
    UDPC_WARNING,
    UDPC_INFO,
    UDPC_VERBOSE,
    UDPC_DEBUG
} UDPC_LoggingType;

typedef enum {
    UDPC_AUTH_POLICY_FALLBACK=0,
    UDPC_AUTH_POLICY_STRICT,
    UDPC_AUTH_POLICY_SIZE /// Used internally to get max size of enum
} UDPC_AuthPolicy;

/*!
 * \brief Data identifying a peer via addr, port, and scope_id
 *
 * This struct needn't be used directly; use UDPC_create_id(),
 * UDPC_create_id_full(), UDPC_create_id_anyaddr(), or UDPC_create_id_easy() to
 * create one. This struct does not hold dynamic data, so there is no need to
 * free it.
 */
typedef struct {
    UDPC_IPV6_ADDR_TYPE addr;
    uint32_t scope_id;
    uint16_t port;
} UDPC_ConnectionId;

/*!
 * \brief Data representing a received/sent packet
 */
typedef struct {
    /*!
     * A char array of size \p UDPC_PACKET_MAX_SIZE. Note that the received data
     * will probably use up less data than the full size of the array. The
     * actual size of the received data is \p dataSize.
     */
    // id is stored at offset 8, size 4 (uint32_t) even for "empty" PktInfos
    char data[UDPC_PACKET_MAX_SIZE];
    /*!
     * \brief Flags indication some additional information about the received
     * packet.
     *
     * The following list indicates what each used bit in \p flags refers to.
     * - 0x1: Is an initiate-connection packet
     * - 0x2: Is a ping packet
     * - 0x4: Is a packet that will not be re-sent if not received
     * - 0x8: Is a packet that was re-sent
     */
    uint32_t flags;
    /*!
     * \brief The size in bytes of the received packet's data inside the \p data
     * array member variable.
     *
     * If this variable is zero, then this packet is invalid, or an empty packet
     * was received.
     */
    uint16_t dataSize;
    /// The \p UDPC_ConnectionId of the sender
    UDPC_ConnectionId sender;
    /// The \p UDPC_ConnectionId of the receiver
    UDPC_ConnectionId receiver;
} UDPC_PacketInfo;

/*!
 * \brief An enum describing the type of event.
 *
 * Note that only the following values will be presented when using
 * UDPC_get_event()
 * - UDPC_ET_NONE: No events have ocurred
 * - UDPC_ET_CONNECTED: A peer has initiated a connection
 * - UDPC_ET_DISCONNECTED: A peer has disconnected
 * - UDPC_ET_GOOD_MODE: The connection has switched to "good mode"
 * - UDPC_ET_BAD_MODE: The connection has switched to "bad mode"
 *
 * The other unmentioned enum values are used internally, and should never be
 * returned in a call to UDPC_get_event().
 */
typedef enum {
    UDPC_ET_NONE,
    UDPC_ET_REQUEST_CONNECT,
    UDPC_ET_REQUEST_DISCONNECT,
    UDPC_ET_CONNECTED,
    UDPC_ET_DISCONNECTED,
    UDPC_ET_GOOD_MODE,
    UDPC_ET_BAD_MODE,
    UDPC_ET_REQUEST_CONNECT_PK
} UDPC_EventType;

/*!
 * \brief A struct containing information related to the type of event
 *
 * Note that instances of this struct received from a call to UDPC_get_event()
 * will not store any useful data in its union member variable \p v (it will
 * only be used internally).
 * Thus, all events received through a call to UDPC_get_event() will contain a
 * valid UDPC_ConnectionId \p conId that identifies the peer that the event is
 * referring to.
 */
typedef struct {
    UDPC_EventType type;
    UDPC_ConnectionId conId;
    union Value {
        int dropAllWithAddr;
        int enableLibSodium;
        unsigned char *pk;
    } v;
} UDPC_Event;

/*!
 * \brief Creates an UDPC_ConnectionId with the given addr and port
 *
 * port should be in native byte order (not network/big-endian). This means that
 * there is no need to convert the 16-bit value to network byte order, this will
 * be done automatically by this library when necessary (without modifying the
 * value in the used UDPC_ConnectionId).
 */
UDPC_ConnectionId UDPC_create_id(UDPC_IPV6_ADDR_TYPE addr, uint16_t port);

/*!
 * \brief Creates an UDPC_ConnectionId with the given addr, scope_id, and port
 *
 * port should be in native byte order (not network/big-endian).
 */
UDPC_ConnectionId UDPC_create_id_full(UDPC_IPV6_ADDR_TYPE addr, uint32_t scope_id, uint16_t port);

/*!
 * \brief Creates an UDPC_ConnectionId with the given port
 *
 * The address contained in the returned UDPC_ConnectionId will be zeroed out
 * (the "anyaddr" address).
 * port should be in native byte order (not network/big-endian).
 */
UDPC_ConnectionId UDPC_create_id_anyaddr(uint16_t port);

/*!
 * \brief Creates an UDPC_ConnectionId with the given addr string and port
 *
 * The address string should be a valid ipv6 or ipv4 address. (If an ipv4
 * address is given, the internal address of the returned UDPC_ConnectionId will
 * be ipv4-mapped ipv6 address.)
 * port should be in native byte order (not network/big-endian).
 */
UDPC_ConnectionId UDPC_create_id_easy(const char *addrString, uint16_t port);

/*!
 * \brief Creates an UDPC_HContext that holds state for connections
 *
 * \param listenId The addr and port to listen on (contained in a
 * UDPC_ConnectionId)
 * \param isClient Whether or not this instance is a client or a server
 * \param isUsingLibsodium Set to non-zero if libsodium verification of packets
 * should be enabled (fails if libsodium support was not compiled)
 *
 * UDPC_is_valid_context() may be used to check if the context was successfully
 * created.
 *
 * \warning The received UDPC_HContext must be freed with a call to UDPC_destroy().
 */
UDPC_HContext UDPC_init(UDPC_ConnectionId listenId, int isClient, int isUsingLibsodium);
/*!
 * \brief Creates an UDPC_HContext that holds state for connections that
 * auto-updates via a thread.
 *
 * By default, the update interval is set to 8 milliseconds.
 *
 * \param listenId The addr and port to listen on (contained in a
 * UDPC_ConnectionId)
 * \param isClient Whether or not this instance is a client or a server
 * \param isUsingLibsodium Set to non-zero if libsodium verification of packets
 * should be enabled (fails if libsodium support was not compiled)
 *
 * UDPC_is_valid_context() may be used to check if the context was successfully
 * created.
 *
 * \warning The received UDPC_HContext must be freed with a call to UDPC_destroy().
 */
UDPC_HContext UDPC_init_threaded_update(
    UDPC_ConnectionId listenId,
    int isClient,
    int isUsingLibsodium);
/*!
 * \brief Creates an UDPC_HContext that holds state for connections that
 * auto-updates via a thread at a specified interval.
 *
 * \param listenId The addr and port to listen on (contained in a
 * UDPC_ConnectionId)
 * \param isClient Whether or not this instance is a client or a server
 * \param updateMS The interval to update at in milliseconds (clamped at a
 * minimum of 4 ms and a maximum of 333 ms)
 * \param isUsingLibsodium Set to non-zero if libsodium verification of packets
 * should be enabled (fails if libsodium support was not compiled)
 *
 * UDPC_is_valid_context() may be used to check if the context was successfully
 * created.
 *
 * \warning The received UDPC_HContext must be freed with a call to UDPC_destroy().
 */
UDPC_HContext UDPC_init_threaded_update_ms(
    UDPC_ConnectionId listenId,
    int isClient,
    int updateMS,
    int isUsingLibsodium);

/*!
 * \brief Enables auto updating on a separate thread for the given UDPC_HContext
 *
 * By default, the update interval is set to 8 milliseconds.
 *
 * \param ctx The context to enable auto updating for
 * \return non-zero if auto updating is enabled. If the context already had auto
 * updating enabled, this function will return zero.
 */
int UDPC_enable_threaded_update(UDPC_HContext ctx);
/*!
 * \brief Enables auto updating on a separate thread for the given UDPC_HContext
 * with the specified update interval
 *
 * \param ctx The context to enable auto updating for
 * \param updateMS The interval to update at in milliseconds (clamped at a
 * minimum of 4 ms and a maximum of 333 ms)
 * \return non-zero if auto updating is enabled. If the context already had auto
 * updating enabled, this function will return zero.
 */
int UDPC_enable_threaded_update_ms(UDPC_HContext ctx, int updateMS);
/*!
 * \brief Disables auto updating on a separate thread for the given
 * UDPC_HContext
 *
 * \param ctx The context to disable auto updating for
 * \return non-zero if auto updating is disabled. If the context already had
 * auto updating disabled, this function will return zero.
 */
int UDPC_disable_threaded_update(UDPC_HContext ctx);

/*!
 * \brief Checks if the given UDPC_HContext is valid (successfully initialized)
 *
 * \return non-zero if the given context is valid
 */
int UDPC_is_valid_context(UDPC_HContext ctx);

/*!
 * \brief Cleans up the UDPC_HContext
 *
 * If auto updating was enabled for the given context, it will gracefully stop
 * the thread before cleaning up the context.
 *
 * \warning This function must be called after a UDPC_HContext is no longer used
 * to avoid memory leaks.
 */
void UDPC_destroy(UDPC_HContext ctx);

/*!
 * \brief Updates the context
 *
 * Updating consists of:
 * - Checking if peers have timed out
 * - Handling requests to connect to server peers as a client
 * - Sending packets to connected peers
 * - Receiving packets from connected peers
 * - Calculating round-trip-time (RTT) to peers
 * - Checking if a peer has not received a packet and queuing that packet to be
 *   resent (this is done by using an ack)
 *
 * If auto updating was enabled for the context, then there is no need to call
 * this function.
 *
 * Note that the context can only receive at most one packet per call to update
 * (due to the fact that UDPC created its UDP socket to not block on receive
 * checks). This is why it is expected to either call this function several
 * times a second (such as in a game's update loop), or have auto-updating
 * enabled via UDPC_init_threaded_update(), UDPC_init_threaded_update_ms(),
 * UDPC_enable_threaded_update(), or UDPC_enable_threaded_update_ms().
 */
void UDPC_update(UDPC_HContext ctx);

/*!
 * \brief Initiate a connection to a server peer
 *
 * Note that this function does nothing on a server context.
 *
 * \param ctx The context to initiate a connection from
 * \param connectionId The server peer to initiate a connection to
 * \param enableLibSodium If packet headers should be verified with the server
 * peer (Fails if UDPC was not compiled with libsodium support)
 */
void UDPC_client_initiate_connection(
    UDPC_HContext ctx,
    UDPC_ConnectionId connectionId,
    int enableLibSodium);

/*!
 * \brief Initiate a connection to a server peer with an expected public key
 *
 * Note that this function does nothing on a server context.
 *
 * \param ctx The context to initiate a connection from
 * \param connectionId The server peer to initiate a connection to
 * \param serverPK A pointer to the public key that the server is expected to
 * use (if the server does not use this public key, then the connection will
 * fail; it must point to a buffer of size \p crypto_sign_PUBLICKEYBYTES)
 *
 * This function assumes that support for libsodium was enabled when UDPC was
 * compiled. If it has not, then this function will fail.
 */
void UDPC_client_initiate_connection_pk(
    UDPC_HContext ctx,
    UDPC_ConnectionId connectionId,
    unsigned char *serverPK);

/*!
 * \brief Queues a packet to be sent to the specified peer
 *
 * Note that there must already be an established connection with the peer. If
 * a packet is queued for a peer that is not connected, it will be dropped and
 * logged with log-level warning. A client can establish a connection to a
 * server peer via a call to UDPC_client_initiate_connection() or
 * UDPC_client_initiate_connection_pk(). A server must receive an
 * initiate-connection-packet from a client to establish a connection (sent by
 * previously mentioned UDPC_client_initiate_* functions).
 *
 * \param ctx The context to send a packet on
 * \param destinationId The peer to send a packet to
 * \param isChecked Set to non-zero if the packet should be re-sent if the peer
 * doesn't receive it
 * \param data A pointer to data to be sent in a packet
 * \param size The size in bytes of the data to be sent
 */
void UDPC_queue_send(UDPC_HContext ctx, UDPC_ConnectionId destinationId,
                     int isChecked, void *data, uint32_t size);

/*!
 * \brief Gets the size of the data structure holding queued packets
 *
 * Note that a UDPC context holds a different data structure per established
 * connection that holds a limited amount of packets to send. If a connection's
 * queue is full, it will not be removed from the main queue that this function
 * (and UDPC_queue_send()) uses. The queue that this function refers to does not
 * have an imposed limit as it is implemented as a thread-safe linked list (data
 * is dynamically stored on the heap) and access to this data structure is
 * faster than accessing a connection's internal queue. Also note that this
 * queue holds packets for all connections this context maintains. Thus if one
 * connection has free space, then it may partially remove packets only destined
 * for that connection from the queue this function refers to.
 */
unsigned long UDPC_get_queue_send_current_size(UDPC_HContext ctx);

/*!
 * \brief Gets the size of a connection's queue of queued packets
 *
 * Note that a UDPC context holds a queue per established connection that holds
 * a limited amount of packets to send. This function checks a connection's
 * internal queue, but must do so after locking an internal mutex (a call to
 * UDPC_update() will lock this mutex, regardless of whether or not the context
 * is using threaded update).
 */
unsigned long UDPC_get_queued_size(UDPC_HContext ctx, UDPC_ConnectionId id, int *exists);

/*!
 * \brief Gets the size limit of a connection's queue of queued packets
 *
 * Note that a call to this function does not use any locks, as the limit is
 * known at compile time and is the same for all UDPC connections.
 */
unsigned long UDPC_get_max_queued_size();

int UDPC_set_accept_new_connections(UDPC_HContext ctx, int isAccepting);

void UDPC_drop_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId, int dropAllWithAddr);

int UDPC_has_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId);

UDPC_ConnectionId* UDPC_get_list_connected(UDPC_HContext ctx, unsigned int *size);

void UDPC_free_list_connected(UDPC_ConnectionId *list);

uint32_t UDPC_get_protocol_id(UDPC_HContext ctx);

uint32_t UDPC_set_protocol_id(UDPC_HContext ctx, uint32_t id);

UDPC_LoggingType UDPC_get_logging_type(UDPC_HContext ctx);

UDPC_LoggingType UDPC_set_logging_type(UDPC_HContext ctx, UDPC_LoggingType loggingType);

int UPDC_get_receiving_events(UDPC_HContext ctx);

int UDPC_set_receiving_events(UDPC_HContext ctx, int isReceivingEvents);

UDPC_Event UDPC_get_event(UDPC_HContext ctx, unsigned long *remaining);

UDPC_PacketInfo UDPC_get_received(UDPC_HContext ctx, unsigned long *remaining);

int UDPC_set_libsodium_keys(UDPC_HContext ctx, unsigned char *sk, unsigned char *pk);

int UDPC_set_libsodium_key_easy(UDPC_HContext ctx, unsigned char *sk);

int UDPC_unset_libsodium_keys(UDPC_HContext ctx);

int UDPC_get_auth_policy(UDPC_HContext ctx);
int UDPC_set_auth_policy(UDPC_HContext ctx, int value);

const char *UDPC_atostr_cid(UDPC_HContext ctx, UDPC_ConnectionId connectionId);

const char *UDPC_atostr(UDPC_HContext ctx, UDPC_IPV6_ADDR_TYPE addr);

/// addrStr must be a valid ipv6 address or a valid ipv4 address
UDPC_IPV6_ADDR_TYPE UDPC_strtoa(const char *addrStr);

UDPC_IPV6_ADDR_TYPE UDPC_strtoa_link(const char *addrStr, uint32_t *linkId_out);

#ifdef __cplusplus
}
#endif
#endif
