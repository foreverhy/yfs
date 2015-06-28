// the caching lock server implementation

#include "lock_server_cache_rsm.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"
#include "marshall.h"


static void *
revokethread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->revoker();
  return 0;
}

static void *
retrythread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->retryer();
  return 0;
}

lock_server_cache_rsm::lock_server_cache_rsm(class rsm *_rsm) 
  : rsm (_rsm)
{
  pthread_t th;
  int r = pthread_create(&th, NULL, &revokethread, (void *) this);
  VERIFY (r == 0);
  r = pthread_create(&th, NULL, &retrythread, (void *) this);
  VERIFY (r == 0);
  rsm->set_state_transfer(this);
}

void
lock_server_cache_rsm::revoker()
{

  // This method should be a continuous loop, that sends revoke
  // messages to lock holders whenever another client wants the
  // same lock
    server_lock *rlk = nullptr;
    for (;;) {
        revoke_fifo.deq(&rlk);
        if (!rsm->amiprimary()) {
            continue;
        }
        handle h(rlk->owner);
        auto cl = h.safebind();
        if (cl){
            int r;
            tprintf("send revoke %llu to %s\n", rlk->lid, rlk->owner.data());
            cl->call(rlock_protocol::revoke, rlk->lid, rlk->xid, r);
        }
    }
}


void
lock_server_cache_rsm::retryer()
{

  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it.
    re_info info;
    for (;;) {
        retry_fifo.deq(&info);
        if (!rsm->amiprimary()) {
            continue;
        }
        handle h(info.dst);
        auto cl = h.safebind();
        if (cl) {
            int r;
            tprintf("send retry %llu to %s\n", info.lid, info.dst.data());
            cl->call(rlock_protocol::retry, info.lid, info.xid, r);
        }
    }
}


int lock_server_cache_rsm::acquire(lock_protocol::lockid_t lid, std::string id, 
             lock_protocol::xid_t xid, int &)
{
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

    if (rlk->owner == id) {
        if (xid < rlk->xid) {
            return lock_protocol::RPCERR;
        }

        if (xid == rlk->xid && server_lock::LOCKED == rlk->status) {
            return lock_protocol::OK;
        }
    }

    if (server_lock::FREE == rlk->status) {
        rlk->status = server_lock::LOCKED;
        rlk->owner = id;
        rlk->xid = xid;
        if (!rlk->retry.empty()) {
            re_info info;
            info.lid = lid;
            info.dst = rlk->retry.front();
            rlk->retry.pop();
            retry_fifo.enq(info);
        }
        return lock_protocol::OK;
    }

    rlk->retry.push(id);
    if (server_lock::LOCKED == rlk->status) {
        rlk->status = server_lock::REVOKE_SENT;
        revoke_fifo.enq(rlk);
    }
    return lock_protocol::RETRY;
}

int 
lock_server_cache_rsm::release(lock_protocol::lockid_t lid, std::string id, 
         lock_protocol::xid_t xid, int &r)
{
    tprintf("%s release %llu\n", id.data(), lid);
    std::unique_lock<std::mutex> mtx_(mtxtb);

    auto iter = locktb.find(lid);
    server_lock *rlk = nullptr;

    if (iter == locktb.end()) {
        return lock_protocol::IOERR;
    } else {
        rlk = iter->second;
    }

    if (id != rlk->owner) {
        return lock_protocol::IOERR;
    }

    if (xid != rlk->xid) {
        return lock_protocol::RPCERR;
    }

    rlk->status = server_lock::FREE;
    rlk->owner.clear();
    rlk->xid = 0;
    if (!rlk->retry.empty()) {
        re_info info; 
        info.lid = lid;
        info.dst = rlk->retry.front();
        info.xid = rlk->xid;
        rlk->retry.pop();
        retry_fifo.enq(info);
    }
    return lock_protocol::OK;
}


std::string
lock_server_cache_rsm::marshal_state()
{
    //std::ostringstream ost;
    //std::string r;
    marshall rep;
    using ull = unsigned long long;
    std::lock_guard<std::mutex> m_(mtxtb);
    rep << static_cast<ull>(locktb.size());
    for (auto &lock : locktb) {
        ull lid = lock.second->lid; 
        int status = lock.second->status; 
        std::string owner = lock.second->owner;
        rep << lid << status << owner;
        ull retrysize = lock.second->retry.size();
        rep << retrysize;
        for (size_t i = 0; i < retrysize; ++i) {
            std::string retryer = lock.second->retry.front();
            rep << retryer; 
            lock.second->retry.pop();
            lock.second->retry.push(retryer);
        }
        ull xid = lock.second->xid;
        rep << xid;
    }
    return rep.str();
}

void
lock_server_cache_rsm::unmarshal_state(std::string state)
{
    unmarshall rep(state);
    using ull = unsigned long long;
    std::lock_guard<std::mutex> m_(mtxtb);
    locktb.clear();
    ull tbsize;
    rep >> tbsize;
    for (size_t i = 0; i < tbsize; ++i) {
        ull lid;
        int status;
        std::string owner;
        ull retrysize;
        rep >> lid >> status >> owner >> retrysize;
        auto rlk = new server_lock(lid);
        rlk->status = status;
        rlk->owner = owner;
        std::string retryer;  
        for (size_t i = 0; i < retrysize; ++i) {
            rep >> retryer;
            rlk->retry.push(retryer);
        }
        ull xid;
        rep >> xid;
        rlk->xid = xid;
        locktb[lid] = rlk;
    }
}

lock_protocol::status
lock_server_cache_rsm::stat(lock_protocol::lockid_t lid, int &r)
{
  printf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

