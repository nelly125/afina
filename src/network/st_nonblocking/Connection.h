#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <memory>
#include <sys/epoll.h>
#include <deque>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <spdlog/logger.h>
#include <protocol/Parser.h>

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> &logger, std::shared_ptr<Afina::Storage> ps)
        : _socket(s), _logger(logger), pStorage(ps) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return _status; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;
    std::deque<std::string> _outgoing;
    bool _status = false;
    static const std::size_t MAX_SIZE = 256;
    char client_buffer[4096] = "";
    std::size_t offset = 0;
    std::size_t offset_write = 0;
    Protocol::Parser parser;
    std::string argument_for_command = "";
    int _head_written_count;

    std::size_t arg_remains = 0;
    std::unique_ptr<Execute::Command> command_to_execute;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
