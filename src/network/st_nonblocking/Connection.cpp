#include "Connection.h"
#include <memory>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <afina/logging/Service.h>
#include <sys/uio.h>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _logger->debug("Start {} socket", _socket);
    _status = true;
    _event.data.fd = _socket;
    _event.data.ptr = this;
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
}

// See Connection.h
void Connection::OnError() {
    _logger->debug("OnError {} socket", _socket);
    _status = false;
}

// See Connection.h
void Connection::OnClose() {
    _logger->debug("OnError {} socket", _socket);
    _status = false;
}

// See Connection.h
void Connection::DoRead() {
    try {
        int readed_bytes = -1;
        if ((readed_bytes = read(_socket, _client_buffer + _offset, sizeof(_client_buffer) - _offset)) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);
            _offset += readed_bytes;
            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (_offset > 0) {
                _logger->debug("Process {} bytes", readed_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(_client_buffer, _offset, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(_client_buffer, _client_buffer + parsed, _offset - parsed);
                        _offset -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                    argument_for_command.append(_client_buffer, to_read);

                    std::memmove(_client_buffer, _client_buffer + to_read, readed_bytes - to_read);
                    arg_remains -= to_read;
                    _offset -= to_read;
                }

                // There is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    if (argument_for_command.size()) {
                        argument_for_command.resize(argument_for_command.size() - 2);
                    }
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Send response
                    result += "\r\n";

                    if (_outgoing.empty()) {
                        _event.events |= EPOLLOUT;
                    }
                    _outgoing.emplace_back(result);
                    if (_outgoing.size() > MAX_SIZE) {
                        _event.events &= ~EPOLLIN;
                    }

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes)
        }

        if (_offset == 0) {
            _logger->debug("Connection closed");
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
        _outgoing.emplace_back("ERROR \r\n");
        if (!(_event.events & EPOLLOUT)) {
            _event.events |= EPOLLOUT;
        }
        _event.events &= ~EPOLLIN;
        OnClose();
    }
}

// See Connection.h
void Connection::DoWrite() {
    _logger->debug("DoWrite {} socket", _socket);

    const size_t size = 64;
    iovec data[size] = {};
    std::size_t i = 0;

    {
        auto it = _outgoing.begin();
        data[i].iov_base = &((*it)[0]) + _offset_write;
        data[i].iov_len = it->size() - _offset_write;
        ++it;
        ++i;
        for (; it != _outgoing.end(); ++it) {
            data[i].iov_base = &((*it)[0]);
            data[i].iov_len = it->size();
            if (++i >= size) {
                break;
            }
        }
    }

    int written = 0;
    if ((written = writev(_socket, data, i)) > 0 ) {
        std::size_t j = 0;
        while (j < i && written >= data[j].iov_len) {
            _outgoing.pop_front();
            written -= data[j].iov_len;
            ++j;
        }
        _offset_write = written;
    } else if (written < 0 && written != EAGAIN) {
        _status = false;
    }
    if (_outgoing.empty()) {
        _event.events &= ~EPOLLOUT;
    }
    if (_outgoing.size() <= MAX_SIZE) {
        _event.events |= EPOLLIN;
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
