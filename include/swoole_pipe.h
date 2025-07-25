/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@swoole.com so we can mail you a copy immediately.            |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <rango@swoole.com>                             |
  |         Twosee  <twose@qq.com>                                       |
  +----------------------------------------------------------------------+
*/

#pragma once

#include "swoole.h"
#include "swoole_socket.h"

enum swPipe_close_which {
    SW_PIPE_CLOSE_MASTER = 1,
    SW_PIPE_CLOSE_WORKER = 2,
    SW_PIPE_CLOSE_READ = 3,
    SW_PIPE_CLOSE_WRITE = 4,
    SW_PIPE_CLOSE_BOTH = 0,
};

namespace swoole {
class SocketPair {
  protected:
    bool blocking;
    double timeout;

    /**
     * master : socks[1], for write operation
     * worker : socks[0], for read operation
     */
    int socks[2]{};

    network::Socket *master_socket = nullptr;
    network::Socket *worker_socket = nullptr;

    bool init_socket(int master_fd, int worker_fd);

  public:
    explicit SocketPair(bool _blocking) {
        blocking = _blocking;
        timeout = network::Socket::default_read_timeout;
    }
    ~SocketPair();

    ssize_t read(void *_buf, size_t length);
    ssize_t write(const void *_buf, size_t length);
    void clean();
    bool close(int which = 0);

    network::Socket *get_socket(bool _master) const {
        return _master ? master_socket : worker_socket;
    }

    bool ready() const {
        return master_socket != nullptr && worker_socket != nullptr;
    }

    void set_timeout(double _timeout) {
        timeout = _timeout;
        master_socket->set_timeout(timeout);
        worker_socket->set_timeout(timeout);
    }

    void set_blocking(bool blocking) const;
};

class Pipe : public SocketPair {
  public:
    explicit Pipe(bool blocking);
};

class UnixSocket : public SocketPair {
    int protocol_;

  public:
    UnixSocket(bool blocking, int _protocol);
    bool set_buffer_size(size_t _size) const;
};

}  // namespace swoole
