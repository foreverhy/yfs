// lock client interface.

#ifndef lock_client_cache_rsm_h

#define lock_client_cache_rsm_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "fifo.h"
#include "lock_client.h"
#include "lang/verify.h"

#include "rsm_client.h"
#include "extent_client.h"

#include <mutex>
#include <condition_variable>
#include <map>
#include <atomic>

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_release: public lock_release_user {
    extent_client *ec;
  public:
    lock_release(extent_client *e): ec(e) {}
    void dorelease(lock_protocol::lockid_t lid) override {
        ec->flush(lid); 
    }
};


class lock_client_cache_rsm;

// Clients that caches locks.  The server can revoke locks using 
// lock_revoke_server.
class lock_client_cache_rsm : public lock_client {
 private:
  rsm_client *rsmc;
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  std::atomic<lock_protocol::xid_t> xid;

  struct client_lock {
      enum xxxstatus {
          FREE,
          LOCKED,
          NONE,
          ACQUIRING,
          RELEASING,
          RETRYING,
      };
      lock_protocol::lockid_t lid;
      lock_protocol::xid_t xid;
      int status = NONE;
      int nrevoke = 0;
      int nretry = 0;
      pthread_t owner = 0;
      std::mutex mtx;
      std::condition_variable lock_cv;
      std::condition_variable revoke_cv;
      std::condition_variable retry_cv;
      client_lock(lock_protocol::lockid_t id): lid(id), xid(0), status(NONE), nrevoke(0), nretry(0){}
  };

  std::map<lock_protocol::lockid_t, client_lock*> rlocktb;
  std::mutex mtxtb;

  lock_protocol::status racquire(client_lock *rlk, std::unique_lock<std::mutex> &m);

  struct release_info {
      lock_protocol::lockid_t lid;
      lock_protocol::xid_t xid;
  };
  fifo<client_lock*> release_fifo;

  pthread_t th;

 public:
  static int last_port;
  lock_client_cache_rsm(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache_rsm();
  lock_protocol::status acquire(lock_protocol::lockid_t);
  virtual lock_protocol::status release(lock_protocol::lockid_t);
  void releaser();
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
				        lock_protocol::xid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
				       lock_protocol::xid_t, int &);
};


#endif
