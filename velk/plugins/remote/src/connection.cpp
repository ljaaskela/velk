#include "connection.h"

#include <cstring>

namespace velk::remote {

namespace {

/// Largest message accepted from a socket; a longer length means a broken or hostile peer.
constexpr uint32_t kMaxMessage = 256u * 1024u * 1024u;

/// How long socket threads wait before checking whether to stop.
constexpr int kPollMs = 100;

} // namespace

void Connection::take(vector<Message>& out)
{
    std::lock_guard lock(mutex_);
    for (auto& m : inbox_) {
        out.push_back(std::move(m));
    }
    inbox_.clear();
}

void Connection::deliver(Message&& message)
{
    std::lock_guard lock(mutex_);
    inbox_.push_back(std::move(message));
}

void LocalConnection::make_pair(std::shared_ptr<Connection>& a, std::shared_ptr<Connection>& b)
{
    auto left = std::make_shared<LocalConnection>();
    auto right = std::make_shared<LocalConnection>();
    left->peer_ = right;
    right->peer_ = left;
    a = left;
    b = right;
}

void LocalConnection::send(const Message& message)
{
    if (!is_open()) {
        return;
    }
    if (auto peer = peer_.lock(); peer && peer->is_open()) {
        peer->deliver(Message(message));
    } else {
        open_ = false;
    }
}

void LocalConnection::close()
{
    open_ = false;
    if (auto peer = peer_.lock()) {
        peer->open_ = false;
    }
}

SocketConnection::SocketConnection(SocketHandle socket) : socket_(socket)
{
    reader_ = std::thread([this] { read_loop(); });
}

SocketConnection::~SocketConnection()
{
    close();
}

void SocketConnection::send(const Message& message)
{
    if (!is_open()) {
        return;
    }
    const auto size = static_cast<uint32_t>(message.size());
    std::lock_guard lock(send_mutex_);
    if (!socket_send(socket_, &size, sizeof(size)) || !socket_send(socket_, message.data(), message.size())) {
        open_ = false;
    }
}

void SocketConnection::close()
{
    if (closed_.exchange(true)) {
        return;
    }
    open_ = false;
    socket_shutdown(socket_);
    if (reader_.joinable()) {
        reader_.join();
    }
    socket_close(socket_);
}

void SocketConnection::read_loop()
{
    // Waits in short slices so close() can stop it: a blocked recv() is not woken by
    // shutdown() on every platform.
    while (open_ && !closed_) {
        bool failed = false;
        if (!socket_readable(socket_, kPollMs, failed)) {
            if (failed) {
                break;
            }
            continue;
        }
        uint32_t size = 0;
        if (!socket_receive(socket_, &size, sizeof(size)) || size > kMaxMessage) {
            break;
        }
        Message message;
        message.resize(size);
        if (size > 0 && !socket_receive(socket_, message.data(), size)) {
            break;
        }
        deliver(std::move(message));
    }
    open_ = false;
}

Listener::~Listener()
{
    stop();
}

uint16_t Listener::start(uint16_t port)
{
    stop();
    uint16_t bound = 0;
    socket_ = socket_listen(port, bound);
    if (socket_ == kNoSocket) {
        return 0;
    }
    stop_ = false;
    thread_ = std::thread([this] { accept_loop(); });
    return bound;
}

void Listener::stop()
{
    stop_ = true;
    if (thread_.joinable()) {
        thread_.join();
    }
    socket_close(socket_);
    socket_ = kNoSocket;

    // Connections accepted but not yet taken would otherwise stay open.
    std::lock_guard lock(mutex_);
    for (auto& c : accepted_) {
        c->close();
    }
    accepted_.clear();
}

void Listener::take(vector<std::shared_ptr<Connection>>& out)
{
    std::lock_guard lock(mutex_);
    for (auto& c : accepted_) {
        out.push_back(std::move(c));
    }
    accepted_.clear();
}

void Listener::accept_loop()
{
    while (!stop_) {
        bool failed = false;
        const SocketHandle s = socket_accept(socket_, kPollMs, failed);
        if (failed) {
            break;
        }
        if (s != kNoSocket) {
            std::lock_guard lock(mutex_);
            accepted_.push_back(std::make_shared<SocketConnection>(s));
        }
    }
}

} // namespace velk::remote
