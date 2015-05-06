//
// Created by mactavish on 15-5-6.
//

#ifndef MIT_6_824_2012_RAII_H
#define MIT_6_824_2012_RAII_H

#include <mutex>

class scope_mtx{
  public:
    scope_mtx(std::mutex &mtx): mtx_(mtx) {
        mtx_.lock();
    }
    ~scope_mtx(){
        mtx_.unlock();
    }
  private:
    std::mutex &mtx_;
};

#endif //MIT_6_824_2012_RAII_H
