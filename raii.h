//
// Created by mactavish on 15-5-6.
//

#ifndef MIT_6_824_2012_RAII_H
#define MIT_6_824_2012_RAII_H

#include "lock_client.h"
#include <fstream>

class scope_lock{
  public:
    scope_lock(lock_client *cl, lock_protocol::lockid_t lid): cl_(cl), lid_(lid) {
        cl_->acquire(lid);
    }
    void unlock(){
        cl_->release(lid_);
    }
    ~scope_lock(){
        cl_->release(lid_);
    }
  private:
    lock_client *cl_;
    lock_protocol::lockid_t lid_;
};


#endif //MIT_6_824_2012_RAII_H
