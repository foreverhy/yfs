// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache_rsm.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include "marshall.h"

#include "rsm_client.h"

#include <chrono>

static void *
releasethread(void *x)
{
  lock_client_cache_rsm *cc = (lock_client_cache_rsm *) x;
  cc->releaser();
  return 0;
}

int lock_client_cache_rsm::last_port = 0;

lock_client_cache_rsm::lock_client_cache_rsm(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache_rsm::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache_rsm::retry_handler);
  xid = 0;
  // You fill this in Step Two, Lab 7
  // - Create rsmc, and use the object to do RPC 
  //   calls instead of the rpcc object of lock_client
  rsmc = new rsm_client(xdst);
  //pthread_t th;
  int r = pthread_create(&th, NULL, &releasethread, (void *) this);
  VERIFY (r == 0);
}

// Caller held the little lock
lock_protocol::status
lock_client_cache_rsm::racquire(client_lock *rlk, std::unique_lock<std::mutex> &m_) {
    int r;
    rlk->status = client_lock::ACQUIRING;

    lock_protocol::xid_t xid_ = ++xid;
    
    for (;;){
        m_.unlock();
        auto ret = rsmc->call(lock_protocol::acquire, rlk->lid, id, xid_, r);
        m_.lock();
        if (lock_protocol::OK == ret){
            rlk->status = client_lock::LOCKED;
            rlk->xid = xid_;
            return lock_protocol::OK;
        } else  if (lock_protocol::RETRY == ret) {
            if (rlk->nretry){
                rlk->nretry = 0;
                continue;
            }
            rlk->retry_cv.wait_for(m_, std::chrono::seconds(3), [&](){ return rlk->nretry;});
            rlk->nretry = 0;
        } else {
            return ret;
        }

    }
}

void
lock_client_cache_rsm::releaser()
{

  // This method should be a continuous loop, waiting to be notified of
  // freed locks that have been revoked by the server, so that it can
  // send a release RPC.
    client_lock *rlk;
    for (;;) {
        release_fifo.deq(&rlk);
        if (rlk->nrevoke < 0) {
            return;
        }
        int r;
        int ret = lock_protocol::OK;
        if (lu) {
            lu->dorelease(rlk->lid);
        }
        ret = rsmc->call(lock_protocol::release, rlk->lid, id, rlk->xid, r);
        std::lock_guard<std::mutex> m_(rlk->mtx);
        rlk->status = client_lock::NONE;
        rlk->lock_cv.notify_all();
    }

}


lock_protocol::status
lock_client_cache_rsm::acquire(lock_protocol::lockid_t lid)
{
    //tprintf("%s ACQUIRE for %llu\n", id.data(), lid);
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
lock_client_cache_rsm::release(lock_protocol::lockid_t lid)
{
    std::unique_lock<std::mutex> mtx_(mtxtb);
    //tprintf("%s RELEASE for %llu\n", id.data(), lid);
    auto iter = rlocktb.find(lid);
    if (iter == rlocktb.end()){
        return lock_protocol::OK;
    }
    client_lock *rlk = iter->second;

    std::unique_lock<std::mutex> m_(rlk->mtx);
    mtx_.unlock();


    if (client_lock::LOCKED != rlk->status){
        return lock_protocol::IOERR;
    }

    //tprintf("RELEASE Phase 1\n");
    if (rlk->nrevoke){
        rlk->status = client_lock::RELEASING;
        --(rlk->nrevoke);
        release_fifo.enq(rlk);
        //tprintf("RELEASE Phase 2\n");
    } else {
        rlk->status = client_lock::FREE;
        rlk->lock_cv.notify_all();
        //tprintf("RELEASE Phase 3\n");
    }
    return lock_protocol::OK;

}


rlock_protocol::status
lock_client_cache_rsm::revoke_handler(lock_protocol::lockid_t lid, 
			          lock_protocol::xid_t xid, int &)
{
    std::unique_lock<std::mutex> mtx_(mtxtb);
    auto iter = rlocktb.find(lid);
    if (iter == rlocktb.end()){
        return lock_protocol::OK;
    }
    client_lock *rlk = iter->second;

    if (xid < rlk->xid) {
        return lock_protocol::RPCERR;
    }

    if (client_lock::FREE == rlk->status || client_lock::NONE == rlk->status){
        rlk->status = client_lock::RELEASING;
        release_fifo.enq(rlk);
        return lock_protocol::OK;
    }


    // Wait when LOCKED or ACQUIRING
    rlk->nrevoke++;
    return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache_rsm::retry_handler(lock_protocol::lockid_t lid, 
			         lock_protocol::xid_t xid, int &)
{
    int ret = rlock_protocol::OK;
    std::unique_lock<std::mutex> mtx_(mtxtb);

    auto iter = rlocktb.find(lid);
    if (iter == rlocktb.end()){
        return lock_protocol::OK;
    }
    client_lock *rlk = iter->second;

    std::unique_lock<std::mutex> m_(rlk->mtx);
    mtx_.unlock();

//    rlk->status = client_lock::NONE;
    rlk->nretry++;
//    rlk->status = client_lock::ACQUIRING;
    rlk->retry_cv.notify_one();

    return ret;
}


lock_client_cache_rsm::~lock_client_cache_rsm(){
    std::unique_lock<std::mutex> mtx_(mtxtb);
    client_lock *rlk = nullptr;
    for (auto iter = rlocktb.begin(); iter != rlocktb.end(); ++iter) {
        rlk = iter->second;
        std::lock_guard<std::mutex> m_(rlk->mtx);
        if (rlk->status == client_lock::RELEASING || rlk->status == client_lock::NONE) {
            continue;
        }
        release_fifo.enq(rlk);
    }
    rlk = new client_lock(0);
    rlk->nrevoke = -1;
    release_fifo.enq(rlk);
     
    pthread_join(th, NULL);
    delete rlk;
}
