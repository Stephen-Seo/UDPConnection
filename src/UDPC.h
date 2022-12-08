/*!
 * \mainpage UDPConnection
 * \ref UDPC.h
 */

/*!
 * \file UDPC.h
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
/// The maximum size of a UDP packet
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

# if UDPC_PLATFORM == UDPC_PLATFORM_WINDOWS
#  define UDPC_EXPORT __declspec(dllexport)
# else
#  define UDPC_EXPORT
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

typedef enum UDPC_EXPORT UDPC_LoggingType {
    /// Does not log anything
    UDPC_SILENT,
    /// Only log errors
    UDPC_ERROR,
    /// Log errors and warnings
    UDPC_WARNING,
    /// Log errors, warnings, and info
    UDPC_INFO,
    /// Log errors, warning, info, and verbose
    UDPC_VERBOSE,
    /// Log all possible types of messages
    UDPC_DEBUG
} UDPC_LoggingType;

/// Note auth policy will only take effect if public key verification of packets
/// is enabled (if libsodium is enabled).
typedef enum UDPC_EXPORT UDPC_AuthPolicy {
    /// All peers will not be denied regardless of use of public key verification
    UDPC_AUTH_POLICY_FALLBACK=0,
    /// Only peers with public key verification will be allowed
    UDPC_AUTH_POLICY_STRICT,
    // Used internally to get max size of enum
    UDPC_AUTH_POLICY_SIZE
} UDPC_AuthPolicy;

/*!
 * \brief Data identifying a peer via addr, port, and scope_id
 *
 * This struct needn't be used directly; use UDPC_create_id(),
 * UDPC_create_id_full(), UDPC_create_id_anyaddr(), or UDPC_create_id_easy() to
 * create one. This struct does not hold dynamic data, so there is no need to
 * free it.
 */
typedef struct UDPC_EXPORT UDPC_ConnectionId {
    UDPC_IPV6_ADDR_TYPE addr;
    uint32_t scope_id;
    uint16_t port;
} UDPC_ConnectionId;

/*!
 * \brief Data representing a received/sent packet
 *
 * If \ref data is NULL or \ref dataSize is 0, then this packet is invalid.
 *
 * \warning This struct must be free'd with a call to UDPC_free_PacketInfo to
 * avoid a memory leak.
 */
typedef struct UDPC_EXPORT UDPC_PacketInfo {
    /*!
     * A char array of size \ref dataSize. Will be NULL if this UDPC_PacketInfo
     * is invalid.
     */
    // id is stored at offset 8, size 4 (uint32_t) even for "empty" PktInfos
    char *data;
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
     * \brief The packet's id.
     *
     * Packets start with id = 0, and will wrap around.
     * This can be used to determine specifically how out of order a packet may
     * be.
     */
    uint32_t id;
    /*!
     * \brief The size in bytes of the received packet's data inside the \ref data
     * pointer member variable.
     *
     * UDPC does not return an empty packet when calling UDPC_get_received(), so
     * in such a packet dataSize shouldn't be zero. (UDPC only stores received
     * packets that do have a payload.) This means that if this variable is 0,
     * then this UDPC_PacketInfo is invalid.
     */
    uint16_t dataSize;
    uint16_t rtt;
    /// The \ref UDPC_ConnectionId of the sender
    UDPC_ConnectionId sender;
    /// The \ref UDPC_ConnectionId of the receiver
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
 * - UDPC_ET_FAIL_CONNECT: Failed to establish a connection to server peer
 * - UDPC_ET_GOOD_MODE: The connection has switched to "good mode"
 * - UDPC_ET_BAD_MODE: The connection has switched to "bad mode"
 *
 * The other unmentioned enum values are used internally, and should never be
 * returned in a call to UDPC_get_event().
 *
 * All events returned by UDPC_get_event() will have set the member variable
 * \p conId in the UDPC_Event which refers to the peer with which the event
 * ocurred.
 */
typedef enum UDPC_EXPORT UDPC_EventType {
    UDPC_ET_NONE,
    UDPC_ET_REQUEST_CONNECT,
    UDPC_ET_REQUEST_DISCONNECT,
    UDPC_ET_CONNECTED,
    UDPC_ET_DISCONNECTED,
    UDPC_ET_FAIL_CONNECT,
    UDPC_ET_GOOD_MODE,
    UDPC_ET_BAD_MODE
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
typedef struct UDPC_EXPORT UDPC_Event {
    UDPC_EventType type;
    UDPC_ConnectionId conId;
    union Value {
        int dropAllWithAddr;
        int enableLibSodium;
    } v;
} UDPC_Event;

/*!
 * \brief Creates an UDPC_ConnectionId with the given addr and port
 *
 * port should be in native byte order (not network/big-endian). This means that
 * there is no need to convert the 16-bit value to network byte order, this will
 * be done automatically by this library when necessary (without modifying the
 * value in the used UDPC_ConnectionId).
 *
 * \return An initialized UDPC_ConnectionId
 */
UDPC_EXPORT UDPC_ConnectionId UDPC_create_id(UDPC_IPV6_ADDR_TYPE addr, uint16_t port);

/*!
 * \brief Creates an UDPC_ConnectionId with the given addr, scope_id, and port
 *
 * port should be in native byte order (not network/big-endian).
 *
 * \return An initialized UDPC_ConnectionId
 */
UDPC_EXPORT UDPC_ConnectionId UDPC_create_id_full(UDPC_IPV6_ADDR_TYPE addr, uint32_t scope_id, uint16_t port);

/*!
 * \brief Creates an UDPC_ConnectionId with the given port
 *
 * The address contained in the returned UDPC_ConnectionId will be zeroed out
 * (the "anyaddr" address).
 * port should be in native byte order (not network/big-endian).
 *
 * \return An initialized UDPC_ConnectionId
 */
UDPC_EXPORT UDPC_ConnectionId UDPC_create_id_anyaddr(uint16_t port);

/*!
 * \brief Creates an UDPC_ConnectionId with the given addr string and port
 *
 * The address string should be a valid ipv6 or ipv4 address. (If an ipv4
 * address is given, the internal address of the returned UDPC_ConnectionId will
 * be ipv4-mapped ipv6 address.)
 * port should be in native byte order (not network/big-endian).
 *
 * \return An initialized UDPC_ConnectionId
 */
UDPC_EXPORT UDPC_ConnectionId UDPC_create_id_easy(const char *addrString, uint16_t port);

UDPC_EXPORT UDPC_ConnectionId UDPC_create_id_hostname(const char *hostname, uint16_t port);

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
 *
 * \return A UDPC context
 */
UDPC_EXPORT UDPC_HContext UDPC_init(UDPC_ConnectionId listenId, int isClient, int isUsingLibsodium);
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
 *
 * \return A UDPC context
 */
UDPC_EXPORT UDPC_HContext UDPC_init_threaded_update(
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
 *
 * \return A UDPC context
 */
UDPC_EXPORT UDPC_HContext UDPC_init_threaded_update_ms(
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
UDPC_EXPORT int UDPC_enable_threaded_update(UDPC_HContext ctx);
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
UDPC_EXPORT int UDPC_enable_threaded_update_ms(UDPC_HContext ctx, int updateMS);
/*!
 * \brief Disables auto updating on a separate thread for the given
 * UDPC_HContext
 *
 * \param ctx The context to disable auto updating for
 * \return non-zero if auto updating is disabled. If the context already had
 * auto updating disabled, this function will return zero.
 */
UDPC_EXPORT int UDPC_disable_threaded_update(UDPC_HContext ctx);

/*!
 * \brief Checks if the given UDPC_HContext is valid (successfully initialized)
 *
 * \return non-zero if the given context is valid
 */
UDPC_EXPORT int UDPC_is_valid_context(UDPC_HContext ctx);

/*!
 * \brief Cleans up the UDPC_HContext
 *
 * If auto updating was enabled for the given context, it will gracefully stop
 * the thread before cleaning up the context.
 *
 * \warning This function must be called after a UDPC_HContext is no longer used
 * to avoid memory leaks.
 */
UDPC_EXPORT void UDPC_destroy(UDPC_HContext ctx);

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
 * Previously, update would only receive one packet per call to update. Now,
 * each individual call to update will process all packets that have been
 * received but haven't been processed yet.
 */
UDPC_EXPORT void UDPC_update(UDPC_HContext ctx);

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
UDPC_EXPORT void UDPC_client_initiate_connection(
    UDPC_HContext ctx,
    UDPC_ConnectionId connectionId,
    int enableLibSodium);

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
UDPC_EXPORT void UDPC_queue_send(UDPC_HContext ctx, UDPC_ConnectionId destinationId,
                     int isChecked, const void *data, uint32_t size);

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
 *
 * \return The size of the queue
 */
UDPC_EXPORT unsigned long UDPC_get_queue_send_current_size(UDPC_HContext ctx);

/*!
 * \brief Gets the size of a connection's queue of queued packets
 *
 * Note that a UDPC context holds a queue per established connection that holds
 * a limited amount of packets to send. This function checks a connection's
 * internal queue, but must do so after locking an internal mutex (a call to
 * UDPC_update() will lock this mutex, regardless of whether or not the context
 * is using threaded update).
 *
 * If \p exists is a non-null pointer to an \p int, and a connection to a peer
 * identified by \p id exists, then the value of \p exists will be set to
 * non-zero, otherwise a non-existing peer will set the value of \p exists to
 * zero.
 *
 * \return The size of a connection's queue
 */
UDPC_EXPORT unsigned long UDPC_get_queued_size(UDPC_HContext ctx, UDPC_ConnectionId id, int *exists);

/*!
 * \brief Gets the size limit of a connection's queue of queued packets
 *
 * Note that a call to this function does not use any locks, as the limit is
 * known at compile time and is the same for all UDPC connections.
 *
 * \return The size limit of a connection's queue
 */
UDPC_EXPORT unsigned long UDPC_get_max_queued_size();

/*!
 * \brief Set whether or not the UDPC context will accept new connections
 * \param ctx The UDPC context
 * \param isAccepting Set to non-zero to accept connections
 * \return The previous setting (1 if accepting, 0 if not)
 */
UDPC_EXPORT int UDPC_set_accept_new_connections(UDPC_HContext ctx, int isAccepting);

/*!
 * \brief Drops an existing connection to a peer
 *
 * Note that UDPC will send a disconnect packet to the peer before removing
 * the internal connection data handling the connection to that peer.
 *
 * \param ctx The UDPC context
 * \param connectionId The identifier of the peer to disconnect from
 * \param dropAllWithAddr Set to non-zero to drop all peers with the ip address
 * specified in \p connectionId
 */
UDPC_EXPORT void UDPC_drop_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId, int dropAllWithAddr);

/*!
 * \brief Checks if a connection exists to the peer identified by the given
 * \p connectionId
 *
 * \param ctx The UDPC context
 * \param connectionId The identifier for a peer
 *
 * \return non-zero if a connection to the peer exists
 */
UDPC_EXPORT int UDPC_has_connection(UDPC_HContext ctx, UDPC_ConnectionId connectionId);

/*!
 * \brief Gets a dynamically allocated array of connected peers' identifiers
 *
 * Note that an additional element is appended to the array that is initialized
 * with all fields to zero.
 *
 * \warning One must call UDPC_free_list_connected() with the returned array to
 * clean up data to avoid a memory leak
 *
 * \param ctx The UDPC context
 * \param size Pointer to an unsigned int to set the size of the returned array
 * (set to NULL to not get a size)
 * \return A dynamically allocated array of identifiers
 */
UDPC_EXPORT UDPC_ConnectionId* UDPC_get_list_connected(UDPC_HContext ctx, unsigned int *size);

/*!
 * \brief Cleans up a dynamically allocated array of connected peers' identifiers
 * \param list The array to clean up
 */
UDPC_EXPORT void UDPC_free_list_connected(UDPC_ConnectionId *list);

/*!
 * \brief Gets the protocol id of the UDPC context
 *
 * UDPC uses the protocol id by prefixing every sent packet with it. Other UDPC
 * instances will only accept packets with the same protocol id.
 *
 * One can use UDPC_set_protocol_id() to change it.
 *
 * \param ctx The UDPC context
 * \return The protocol id of the given UDPC context
 */
UDPC_EXPORT uint32_t UDPC_get_protocol_id(UDPC_HContext ctx);

/*!
 * \brief Sets the protocol id of the UDPC context
 *
 * UDPC uses the protocol id by prefixing every sent packet with it. Other UDPC
 * instances will only accept packets with the same protocol id.
 *
 * \param ctx The UDPC context
 * \param id The new id to use as the protocol id
 * \return The previous protocol id of the UDPC context
 */
UDPC_EXPORT uint32_t UDPC_set_protocol_id(UDPC_HContext ctx, uint32_t id);

/*!
 * \brief Gets the logging type of the UDPC context
 *
 * See \ref UDPC_LoggingType for possible values.
 *
 * \param ctx The UDPC context
 * \return The logging type of the UDPC context
 */
UDPC_EXPORT UDPC_LoggingType UDPC_get_logging_type(UDPC_HContext ctx);

/*!
 * \brief Sets the logging type of the UDPC context
 *
 * See \ref UDPC_LoggingType for possible values.
 *
 * \param ctx The UDPC context
 * \param loggingType The logging type to set to
 * \return The previously set logging type
 */
UDPC_EXPORT UDPC_LoggingType UDPC_set_logging_type(UDPC_HContext ctx, UDPC_LoggingType loggingType);

/*!
 * \brief Returns non-zero if the UDPC context will record events
 *
 * Events that have ocurred can by polled by calling UDPC_get_event()
 *
 * \param ctx The UDPC context
 * \return non-zero if receiving events
 */
UDPC_EXPORT int UDPC_get_receiving_events(UDPC_HContext ctx);

/*!
 * \brief Sets whether or not UDPC will record events
 *
 * Events that have ocurred can by polled by calling UDPC_get_event()
 *
 * \param ctx The UDPC context
 * \param isReceivingEvents Set to non-zero to receive events
 * \return non-zero if UDPC was previously receiving events
 */
UDPC_EXPORT int UDPC_set_receiving_events(UDPC_HContext ctx, int isReceivingEvents);

/*!
 * \brief Gets a recorded event
 *
 * See \ref UDPC_EventType for possible types of a UDPC_Event.
 *
 * \param ctx The UDPC context
 * \param remaining Pointer to set the number of remaining events that can be
 * returned
 * \return An UDPC_Event (will be of type UDPC_ET_NONE if there are no more
 * events)
 */
UDPC_EXPORT UDPC_Event UDPC_get_event(UDPC_HContext ctx, unsigned long *remaining);

/*!
 * \brief Get a received packet from a given UDPC context.
 *
 * \warning The received packet (if valid) must be free'd with a call to
 * \ref UDPC_free_PacketInfo() to avoid a memory leak.
 */
UDPC_EXPORT UDPC_PacketInfo UDPC_get_received(UDPC_HContext ctx, unsigned long *remaining);

/*!
 * \brief Frees a UDPC_PacketInfo.
 *
 * Internally, the member variable \ref UDPC_PacketInfo::data will be free'd and
 * set to NULL and \ref UDPC_PacketInfo::dataSize will be set to 0 if the given
 * packet is valid.
 */
UDPC_EXPORT void UDPC_free_PacketInfo(UDPC_PacketInfo pInfo);

/*!
 * \brief Sets public/private keys used for packet verification
 *
 * If keys are not set and packet verification is enabled, for each new
 * connection new keys will be generated then used. The auto-generated keys
 * used will be unique per connection. Conversely if keys are set, then new
 * connections will use the given keys.
 *
 * Note that connections established before calling this function will not use
 * the given keys.
 *
 * Note that public key verification will not occur if it is not enabled during
 * the call to UDPC_init().
 *
 * \return Non-zero if keys were successfully set, zero if context is invalid or
 * libsodium is not enabled
 */
UDPC_EXPORT int UDPC_set_libsodium_keys(UDPC_HContext ctx, const unsigned char *sk, const unsigned char *pk);

/*!
 * \brief Sets the public/private keys used for packet verification
 *
 * This function is almost identical with UDPC_set_libsodium_keys, except it
 * will utilize libsodium to generate the associated public key with the given
 * private key.
 *
 * Note that public key verification will not occur if it is not enabled during
 * the call to UDPC_init().
 *
 * \return Non-zero if keys were successfully set, zero if context is invalid or
 * libsodium is not enabled
 */
UDPC_EXPORT int UDPC_set_libsodium_key_easy(UDPC_HContext ctx, const unsigned char *sk);

/*!
 * \brief Removes set keys if any used for packet verification
 *
 * Note that public key verification will not occur if it is not enabled during
 * the call to UDPC_init().
 *
 * \return Zero if context is invalid or libsodium is not enabled
 */
UDPC_EXPORT int UDPC_unset_libsodium_keys(UDPC_HContext ctx);

/*!
 * \brief Adds a public key to the whitelist
 *
 * By default the whitelist is empty and any peer regardless of key will not be
 * denied connection.
 *
 * This function adds one public key to the whitelist. If the whitelist is not
 * empty, then all peers that do not have the matching public key will be
 * denied connection.
 *
 * Note that public key verification will not occur if it is not enabled during
 * the call to UDPC_init().
 *
 * \return The size of the whitelist on success, zero otherwise
 */
UDPC_EXPORT int UDPC_add_whitelist_pk(UDPC_HContext ctx, const unsigned char *pk);

/*!
 * \brief Checks if a public key is in the whitelist
 *
 * Note that public key verification will not occur if it is not enabled during
 * the call to UDPC_init().
 *
 * \return Non-zero if the given public key is in the whitelist
 */
UDPC_EXPORT int UDPC_has_whitelist_pk(UDPC_HContext ctx, const unsigned char *pk);

/*!
 * \brief Removes a public key from the whitelist
 *
 * Note that public key verification will not occur if it is not enabled during
 * the call to UDPC_init().
 *
 * \return Non-zero if a public key was removed
 */
UDPC_EXPORT int UDPC_remove_whitelist_pk(UDPC_HContext ctx, const unsigned char *pk);

/*!
 * \brief Clears the public key whitelist
 *
 * If the whitelist is empty, then no connections will be denied.
 *
 * If there are keys in the whitelist, then new connections will only be allowed
 * if the peer uses a public key in the whitelist.
 *
 * Note that public key verification will not occur if it is not enabled during
 * the call to UDPC_init().
 *
 * \return Zero if the context is invalid or libsodium is not enabled, non-zero
 * if the whitelist was successfully cleared
 */
UDPC_EXPORT int UDPC_clear_whitelist(UDPC_HContext ctx);

/*!
 * \brief Gets how peers are handled regarding public key verification
 *
 * If libsodium is enabled and the auth policy is "strict", then peers
 * attempting to connect will be denied if they do not have public key
 * verification enabled. Otherwise if the auth policy is "fallback", then peers
 * will not be denied a connection regardless of whether or not they use
 * public key verification of packets.
 *
 * Note that public key verification will not occur if it is not enabled during
 * the call to UDPC_init().
 *
 * \return The current auth policy (see \ref UDPC_AuthPolicy) , or zero on fail
 */
UDPC_EXPORT int UDPC_get_auth_policy(UDPC_HContext ctx);

/*!
 * \brief Sets how peers are handled regarding public key verification
 *
 * If libsodium is enabled and the auth policy is "strict", then peers
 * attempting to connect will be denied if they do not have public key
 * verification enabled. Otherwise if the auth policy is "fallback", then peers
 * will not be denied a connection regardless of whether or not they use
 * public key verification of packets.
 *
 * Note that public key verification will not occur if it is not enabled during
 * the call to UDPC_init().
 *
 * \return The previous auth policy (see \ref UDPC_AuthPolicy), or zero on fail
 */
UDPC_EXPORT int UDPC_set_auth_policy(UDPC_HContext ctx, int value);

UDPC_EXPORT const char *UDPC_atostr_cid(UDPC_HContext ctx, UDPC_ConnectionId connectionId);

UDPC_EXPORT const char *UDPC_atostr(UDPC_HContext ctx, UDPC_IPV6_ADDR_TYPE addr);

// =============================================================================
// Helpers

/// addrStr must be a valid ipv6 address or a valid ipv4 address
UDPC_EXPORT UDPC_IPV6_ADDR_TYPE UDPC_strtoa(const char *addrStr);

UDPC_EXPORT UDPC_IPV6_ADDR_TYPE UDPC_strtoa_link(const char *addrStr, uint32_t *linkId_out);

UDPC_EXPORT UDPC_IPV6_ADDR_TYPE UDPC_a4toa6(uint32_t a4_be);

UDPC_EXPORT int UDPC_is_big_endian();
UDPC_EXPORT uint16_t UDPC_no16i(uint16_t i);
UDPC_EXPORT uint32_t UDPC_no32i(uint32_t i);
UDPC_EXPORT uint64_t UDPC_no64i(uint64_t i);
UDPC_EXPORT float UDPC_no32f(float f);
UDPC_EXPORT double UDPC_no64f(double f);

#ifdef __cplusplus
}
#endif
#endif
