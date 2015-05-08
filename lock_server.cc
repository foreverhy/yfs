// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r) {
    printf("acquire request for lock %llu from clt %d\n", lid, clt);
    locks_.lock(lid);
    return lock_protocol::OK;
}


lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r){
    printf("release request for lock %llu from clt %d\n", lid, clt);
    locks_.unlock(lid);
    return lock_protocol::OK;
}
