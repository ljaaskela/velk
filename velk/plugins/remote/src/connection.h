#ifndef VELK_REMOTE_CONNECTION_H
#define VELK_REMOTE_CONNECTION_H

#include "socket.h"

#include <velk/vector.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace velk::remote {

using Message = vector<uint8_t>;

/**
 * @brief One end of a link that carries encoded messages.
 *
 * send() and take() are called from the owner thread; transports deliver
 * from their own threads.
 */
class Connection
{
public:
    virtual ~Connection() = default;

    /** @brief Sends one encoded message. */
    virtual void send(const Message& message) = 0;

    /** @brief Closes the link; both ends then report !is_open(). */
    virtual void close() = 0;

    bool is_open() const { return open_.load(); }

    /** @brief Moves the received messages, oldest first, to @p out. */
    void take(vector<Message>& out);

protected:
    void deliver(Message&& message);

    std::atomic<bool> open_{true};

private:
    std::mutex mutex_;
    vector<Message> inbox_;
};

/** @brief Two connected ends in one process. Messages are copied, never shared. */
class LocalConnection final : public Connection
{
public:
    static void make_pair(std::shared_ptr<Connection>& a, std::shared_ptr<Connection>& b);

    void send(const Message& message) override;
    void close() override;

private:
    std::weak_ptr<LocalConnection> peer_;
};

/** @brief A TCP connection. Messages are framed by a 32-bit length; a thread reads them. */
class SocketConnection final : public Connection
{
public:
    explicit SocketConnection(SocketHandle socket);
    ~SocketConnection() override;

    void send(const Message& message) override;
    void close() override;

private:
    void read_loop();

    SocketHandle socket_;
    std::thread reader_;
    std::mutex send_mutex_;
    std::atomic<bool> closed_{false};
};

/** @brief Accepts TCP connections on a thread. */
class Listener
{
public:
    ~Listener();

    /** @brief Listens on @p port of localhost (0 picks one). Returns the port, or 0 on failure. */
    uint16_t start(uint16_t port);
    void stop();

    /** @brief Moves the connections accepted since the last call to @p out. */
    void take(vector<std::shared_ptr<Connection>>& out);

private:
    void accept_loop();

    SocketHandle socket_ = kNoSocket;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::mutex mutex_;
    vector<std::shared_ptr<Connection>> accepted_;
};

} // namespace velk::remote

#endif // VELK_REMOTE_CONNECTION_H
