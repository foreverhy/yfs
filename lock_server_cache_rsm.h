#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>

#include "lock_protocol.h"
#include "rpc.h"
#include "rsm_state_transfer.h"
#include "rsm.h"
#include "fifo.h"

#include <mutex>
#include <condition_variable>
#include <queue>

class lock_server_cache_rsm : public rsm_state_transfer {
 private:
  int nacquire;
  class rsm *rsm;
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
      lock_protocol::xid_t xid;

      server_lock(lock_protocol::lockid_t id): lid(id), status(FREE), xid(0){}
  };
  std::map<lock_protocol::lockid_t, server_lock*> locktb;
  std::mutex mtxtb;

  struct re_info {
    lock_protocol::xid_t xid;
    lock_protocol::lockid_t lid;
    std::string dst;
  };

  fifo<server_lock*> revoke_fifo;
  fifo<re_info> retry_fifo;

 public:
  lock_server_cache_rsm(class rsm *rsm = 0);
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  void revoker();
  void retryer();
  std::string marshal_state();
  void unmarshal_state(std::string state);
  int acquire(lock_protocol::lockid_t, std::string id, 
	      lock_protocol::xid_t, int &);
  int release(lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
	      int &);
};

#endif
