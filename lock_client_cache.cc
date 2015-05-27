// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst,
                                     class lock_release_user *_lu)
        : lock_client(xdst), lu(_lu) {
    rpcs *rlsrpc = new rpcs(0);
    rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
    rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

    const char *hname;
    hname = "127.0.0.1";
    std::ostringstream host;
    host << hname << ":" << rlsrpc->port();
    rlock_port = rlsrpc->port();
    id = host.str();
}

// Caller host the little lock
lock_protocol::status
lock_client_cache::racquire(client_lock *rlk, std::unique_lock<std::mutex> &m_) {
    int r;
    rlk->status = client_lock::ACQUIRING;
    for (;;){
        m_.unlock();
        auto ret = cl->call(lock_protocol::acquire, rlk->lid, id, r);
        m_.lock();
        if (lock_protocol::OK == ret){
            rlk->status = client_lock::LOCKED;
            return lock_protocol::OK;
        } else  if (lock_protocol::RETRY == ret) {
            if (rlk->nretry){
                rlk->nretry = 0;
                continue;
            }
            rlk->retry_cv.wait(m_, [&](){ return rlk->nretry;});
            rlk->nretry = 0;
        } else {
            return ret;
        }

    }
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid) {
    int ret = lock_protocol::OK;
    std::unique_lock<std::mutex> mtx_(mtxtb);
    client_lock *rlk = nullptr;
    auto iter = rlocktb.find(lid);
    if (iter == rlocktb.end()){
        rlk = new client_lock(lid);
        rlocktb[lid] = rlk;
    } else {
        rlk = iter->second;
    }


    std::unique_lock<std::mutex> m_(rlk->mtx);
    tprintf("%d ACCQURE::got little mtx %llu, %p nrevoke %d\n", rlock_port, rlk->lid, rlk, rlk->nrevoke);
    mtx_.unlock();

    // LOCKED, ACQUIRING, RELEASING wait on lock_cv
    if (client_lock::LOCKED == rlk->status || client_lock::ACQUIRING == rlk->status || client_lock::RELEASING == rlk->status){
        rlk->lock_cv.wait(m_, [&](){ return client_lock::FREE == rlk->status || client_lock::NONE == rlk->status;});
    }

    if (client_lock::NONE == rlk->status) {
        return racquire(rlk, m_);
    }
    if (client_lock::FREE == rlk->status) {
        rlk->status = client_lock::LOCKED;
        return lock_protocol::OK;
    }

    return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) {
    std::unique_lock<std::mutex> mtx_(mtxtb);
    auto iter = rlocktb.find(lid);
    if (iter == rlocktb.end()){
        return lock_protocol::OK;
    }
    client_lock *rlk = iter->second;

    std::unique_lock<std::mutex> m_(rlk->mtx);
    tprintf("%d RELEASE::got little mtx %llu, %p nrevoke %d\n", rlock_port, rlk->lid, rlk, rlk->nrevoke);
    mtx_.unlock();


    if (client_lock::LOCKED != rlk->status){
        return lock_protocol::IOERR;
    }

    if (rlk->nrevoke){
        rlk->status = client_lock::RELEASING;
        rlk->nrevoke--;
        tprintf("%d RELEASE::Wake revoke_cv %llu\n", rlock_port, lid);
        rlk->revoke_cv.notify_all();
    } else {
        tprintf("%d RELEASE::Change state to FREE %llu\n", rlock_port, lid);
        rlk->status = client_lock::FREE;
        rlk->lock_cv.notify_all();
    }
    return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid,
                                  int &) {
    tprintf("%d RECV revoke for %llu\n", rlock_port, lid);

    std::unique_lock<std::mutex> mtx_(mtxtb);
    auto iter = rlocktb.find(lid);
    if (iter == rlocktb.end()){
        tprintf("%d RECV revoke done %llu\n", rlock_port, lid);
        return lock_protocol::OK;
    }
    client_lock *rlk = iter->second;


    if (client_lock::FREE == rlk->status || client_lock::NONE == rlk->status){
        rlk->status = client_lock::NONE;
        tprintf("%d RECV revoke done %llu\n",rlock_port, lid);
        return lock_protocol::OK;
    }


    // Wait when LOCKED or ACQUIRING
    rlk->nrevoke++;
    std::unique_lock<std::mutex> m_(rlk->mtx);
    tprintf("%d REVOKE::got little mtx %llu, %p nrevoke %d\n", rlock_port, rlk->lid, rlk, rlk->nrevoke);
    mtx_.unlock();
    rlk->revoke_cv.wait(m_, [&](){ return client_lock::RELEASING == rlk->status;});

    tprintf("%d RECV revoke done %llu\n", rlock_port, lid);
    rlk->status = client_lock::NONE;
    rlk->lock_cv.notify_all();
    return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid,
                                 int &) {
    int ret = rlock_protocol::OK;
    std::unique_lock<std::mutex> mtx_(mtxtb);

    auto iter = rlocktb.find(lid);
    if (iter == rlocktb.end()){
        return lock_protocol::OK;
    }
    client_lock *rlk = iter->second;

    std::unique_lock<std::mutex> m_(rlk->mtx);
    tprintf("%d RETRY got little mtx %llu\n", rlock_port, lid);
    mtx_.unlock();

//    rlk->status = client_lock::NONE;
    rlk->nretry++;
//    rlk->status = client_lock::ACQUIRING;
    rlk->retry_cv.notify_one();

    return ret;
}

lock_client_cache::~lock_client_cache(){
    std::unique_lock<std::mutex> mtx_(mtxtb);
    client_lock *rlk = nullptr;
    for (auto iter = rlocktb.begin(); iter != rlocktb.end(); ++iter) {
        rlk = iter->second;
        int r;
        cl->call(lock_protocol::release, rlk->lid, id, r);
        delete rlk;
    }
}

