//
// Created by mactavish on 15-5-6.
//

#include "locks.h"
#include "raii.h"


locks::state locks::lookup(lock_protocol::lockid_t lockid) {
    std::lock_guard<std::mutex> smtx_(mtx);
    try {
        return table.at(lockid);
    } catch (std::out_of_range &e) {
        return locks::FREE;
    }
}

bool locks::lock(lock_protocol::lockid_t lockid) {
    std::unique_lock<std::mutex> lk(mtx);
    auto ptb = table.find(lockid);
    if (ptb == table.end()){
        table[lockid] = locks::LOCKED;
        return true;
    }

    cv.wait(lk, [&](){return locks::FREE == table[lockid];});

    table[lockid] = locks::LOCKED;
    return true;
}

bool locks::unlock(lock_protocol::lockid_t lockid) {
    std::unique_lock<std::mutex> lk(mtx);

    auto ptb = table.find(lockid);
    if (ptb == table.end() || locks::FREE == ptb->second){
        return false;
    }

    table[lockid] = locks::FREE;
    cv.notify_all();
    return true;
}
