#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <queue>
#include <map>
#include <mutex>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
  private:
    int nacquire;

    struct server_lock {
        enum xxxstatus {
            FREE,
            LOCKED,
            REVOKE_SENT,
        };
        lock_protocol::lockid_t lid;
        int status;
        std::string owner;
        std::queue<std::string> retry;

        server_lock(lock_protocol::lockid_t id): lid(id), status(FREE){}
    };

    std::map<lock_protocol::lockid_t, server_lock*> locktb;
    std::mutex mtxtb;

  public:
    lock_server_cache();

    lock_protocol::status stat(lock_protocol::lockid_t, int &);

    int acquire(lock_protocol::lockid_t, std::string id, int &);

    int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
