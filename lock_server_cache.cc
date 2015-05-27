// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache() : nacquire(0) {
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &) {
    tprintf("%s acquire for %llu\n", id.data(), lid);
    std::unique_lock<std::mutex> mtx_(mtxtb);
    auto iter = locktb.find(lid);
    server_lock *rlk = nullptr;
    if (iter == locktb.end()) {
        rlk = new server_lock(lid);
        locktb[lid] = rlk;
    } else {
        rlk = iter->second;
    }

    if (server_lock::FREE == rlk->status) {
        rlk->status = server_lock::LOCKED;
        rlk->owner = id;
        return lock_protocol::OK;
    }

    if (server_lock::LOCKED == rlk->status){
        handle h(rlk->owner);
        rpcc *cl = h.safebind();
        if (cl){
            int r;
            rlk->status = server_lock::REVOKE_SENT;
            tprintf("REVOKE %llu on %s\n", lid, rlk->owner.data());
            mtx_.unlock();
            auto ret = cl->call(rlock_protocol::revoke, lid, r);
            mtx_.lock();
            if (lock_protocol::OK == ret){
                rlk->status = server_lock::LOCKED;
                rlk->owner = id;
                tprintf("LOCK %llu by %s\n", lid, rlk->owner.data());
            } else {
                return ret;
            }
            if (!rlk->retry.empty()){
                std::string cid = rlk->retry.front();
                rlk->retry.pop();
                mtx_.unlock();
                handle hretry(cid);
                cl = hretry.safebind();
                if (cl) {
                    tprintf("SEND retry %llu to %s\n", lid, cid.data());
                    cl->call(rlock_protocol::retry, lid, r);
                    tprintf("retry %llu to %s done\n", lid, cid.data());
                }
            }
            return lock_protocol::OK;
        } else {
            return lock_protocol::RPCERR;
        }
    } else {
        rlk->retry.push(id);
        tprintf("send RETRY %llu to %s\n", lid, id.data());
        return lock_protocol::RETRY;
    }

}

int
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id,
                           int &r) {

    printf("%s release %llu\n", id.data(), lid);
    std::unique_lock<std::mutex> mtx_(mtxtb);

    auto iter = locktb.find(lid);
    server_lock *rlk = nullptr;
    if (iter == locktb.end()) {
        rlk = new server_lock(lid);
    } else {
        rlk = iter->second;
    }

    rlk->status = server_lock::FREE;

    // The lock is free now, we should send retry to the waiting clients (if any).
    if (rlk->retry.empty()){
        return lock_protocol::OK;
    }

    auto cid = rlk->retry.front();
    rlk->retry.pop();
    // Since RPC call is slow, we release the global mtx
    mtx_.unlock();
    handle h(id);
    rpcc *cl = h.safebind();
    if (cl){
        int r;
        cl->call(rlock_protocol::retry, lid, r);
        return lock_protocol::OK;
    } else {
        return lock_protocol::RPCERR;
    }
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r) {
    tprintf("stat request\n");
    r = nacquire;
    return lock_protocol::OK;
}

