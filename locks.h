//
// Created by mactavish on 15-5-6.
//

#ifndef MIT_6_824_2012_LOCK_H
#define MIT_6_824_2012_LOCK_H

#include <map>
#include <mutex>
#include <condition_variable>
#include "lock_protocol.h"

class locks {
  public:
    enum state {
      FREE,
      LOCKED
    };
    locks(){};
    ~locks(){};
    locks::state lookup(lock_protocol::lockid_t lockid);
    bool lock(lock_protocol::lockid_t lockid);
    bool unlock(lock_protocol::lockid_t lockid);
  private:
    std::map<lock_protocol::lockid_t, locks::state> table;
    std::mutex mtx;
    std::condition_variable cv;


};

#endif //MIT_6_824_2012_LOCK_H
