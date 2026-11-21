#include "socket.h"

#include <velk/string.h>

#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace velk::remote {

namespace {

#ifdef _WIN32
using Native = SOCKET;
bool valid(Native s) { return s != INVALID_SOCKET; }
void close_native(Native s) { closesocket(s); }
constexpr int kShutBoth = SD_BOTH;
#else
using Native = int;
bool valid(Native s) { return s >= 0; }
void close_native(Native s) { ::close(s); }
constexpr int kShutBoth = SHUT_RDWR;
#endif

Native native(SocketHandle s) { return static_cast<Native>(s); }
SocketHandle handle(Native s) { return valid(s) ? static_cast<SocketHandle>(s) : kNoSocket; }

void set_no_delay(Native s)
{
    int on = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
}

} // namespace

bool sockets_start()
{
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return true;
#endif
}

void sockets_stop()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

SocketHandle socket_listen(uint16_t port, uint16_t& bound)
{
    Native s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!valid(s)) {
        return kNoSocket;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || ::listen(s, 8) != 0) {
        close_native(s);
        return kNoSocket;
    }
    socklen_t len = sizeof(addr);
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        close_native(s);
        return kNoSocket;
    }
    bound = ntohs(addr.sin_port);
    return handle(s);
}

bool socket_readable(SocketHandle socket, int timeout_ms, bool& failed)
{
    failed = false;
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(native(socket), &readable);
    timeval timeout{};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    const int ready = ::select(static_cast<int>(native(socket)) + 1, &readable, nullptr, nullptr, &timeout);
    failed = ready < 0;
    return ready > 0;
}

SocketHandle socket_accept(SocketHandle listener, int timeout_ms, bool& failed)
{
    if (!socket_readable(listener, timeout_ms, failed)) {
        return kNoSocket;
    }
    Native s = ::accept(native(listener), nullptr, nullptr);
    failed = !valid(s);
    if (valid(s)) {
        set_no_delay(s);
    }
    return handle(s);
}

SocketHandle socket_connect(string_view host, uint16_t port)
{
    const string host_z(host);
    char port_z[8];
    std::snprintf(port_z, sizeof(port_z), "%u", static_cast<unsigned>(port));

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* found = nullptr;
    if (::getaddrinfo(host_z.c_str(), port_z, &hints, &found) != 0 || !found) {
        return kNoSocket;
    }
    Native s = ::socket(found->ai_family, found->ai_socktype, found->ai_protocol);
    if (valid(s) && ::connect(s, found->ai_addr, static_cast<int>(found->ai_addrlen)) != 0) {
        close_native(s);
        s = native(kNoSocket);
    }
    ::freeaddrinfo(found);
    if (valid(s)) {
        set_no_delay(s);
    }
    return handle(s);
}

bool socket_send(SocketHandle socket, const void* data, size_t size)
{
    const char* p = static_cast<const char*>(data);
    while (size > 0) {
        const int chunk = size > 1u << 30 ? 1 << 30 : static_cast<int>(size);
        const auto sent = ::send(native(socket), p, chunk, 0);
        if (sent <= 0) {
            return false;
        }
        p += sent;
        size -= static_cast<size_t>(sent);
    }
    return true;
}

bool socket_receive(SocketHandle socket, void* data, size_t size)
{
    char* p = static_cast<char*>(data);
    while (size > 0) {
        const int chunk = size > 1u << 30 ? 1 << 30 : static_cast<int>(size);
        const auto got = ::recv(native(socket), p, chunk, 0);
        if (got <= 0) {
            return false;
        }
        p += got;
        size -= static_cast<size_t>(got);
    }
    return true;
}

void socket_shutdown(SocketHandle socket)
{
    if (socket != kNoSocket) {
        ::shutdown(native(socket), kShutBoth);
    }
}

void socket_close(SocketHandle socket)
{
    if (socket != kNoSocket) {
        close_native(native(socket));
    }
}

} // namespace velk::remote
