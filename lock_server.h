// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "locks.h"
#include "rpc.h"

class lock_server {

  protected:
    int nacquire;
    locks locks_;

  public:
    lock_server();

    ~lock_server() { };

    lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
    lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &r);
    lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &r);

};

#endif 







