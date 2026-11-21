#ifndef VELK_REMOTE_SOCKET_H
#define VELK_REMOTE_SOCKET_H

#include <velk/string_view.h>

#include <cstddef>
#include <cstdint>

namespace velk::remote {

/** @brief A TCP socket: a SOCKET on Windows, a file descriptor elsewhere. */
using SocketHandle = intptr_t;
inline constexpr SocketHandle kNoSocket = -1;

/** @brief Starts the platform's socket library. Pair with sockets_stop(). */
bool sockets_start();
void sockets_stop();

/** @brief Listens on @p port of 127.0.0.1 (0 picks one). @p bound receives the port. */
SocketHandle socket_listen(uint16_t port, uint16_t& bound);

/**
 * @brief Waits up to @p timeout_ms for a connection.
 * @return The connection, or kNoSocket on timeout (@p failed false) or error (@p failed true).
 */
SocketHandle socket_accept(SocketHandle listener, int timeout_ms, bool& failed);

/** @brief Connects to @p host and @p port. */
SocketHandle socket_connect(string_view host, uint16_t port);

/**
 * @brief Waits up to @p timeout_ms for @p socket to have data (or be closed by the peer).
 * @return True when readable; false on timeout (@p failed false) or error (@p failed true).
 */
bool socket_readable(SocketHandle socket, int timeout_ms, bool& failed);

/** @brief Sends all @p size bytes. Returns false on a closed or failed socket. */
bool socket_send(SocketHandle socket, const void* data, size_t size);

/** @brief Receives exactly @p size bytes. Returns false on a closed or failed socket. */
bool socket_receive(SocketHandle socket, void* data, size_t size);

/** @brief Ends sending and receiving; the peer sees the connection close. */
void socket_shutdown(SocketHandle socket);

/** @brief Closes @p socket. */
void socket_close(SocketHandle socket);

} // namespace velk::remote

#endif // VELK_REMOTE_SOCKET_H
