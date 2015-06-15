// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

namespace {

inline bool isfile(extent_protocol::extentid_t id) {
    if (id & 0x80000000) {
        return true;
    }
    return false;
}

inline bool isdir(extent_protocol::extentid_t id) {
    return !isfile(id);
}

}


// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst) {
    sockaddr_in dstsock;
    make_sockaddr(dst.c_str(), &dstsock);
    cl = new rpcc(dstsock);
    if (cl->bind() != 0) {
        printf("extent_client: bind failed\n");
    }
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf) {
    decltype(cache_.begin()) iter;
    {
        std::unique_lock<std::mutex> m_(cache_mtx_);
        iter = cache_.find(eid);
        if (cache_.end() != iter && (iter->second.status == extent_protocol::ALL_CACHED) ) {
            buf = iter->second.buf;
            iter->second.attr.atime = std::time(nullptr);
            return extent_protocol::OK;
        }
    }

    cl->call(extent_protocol::get, eid, buf);

    {
        extent_protocol::attr attr;
        getattr(eid, attr);
        std::unique_lock<std::mutex> m_(cache_mtx_);
        if (cache_.end() != iter) {
            iter->second.buf = buf;
            iter->second.attr.atime = std::time(nullptr);
            iter->second.status = extent_protocol::ALL_CACHED;
        } else {
            cache_[eid] = {eid, attr, buf, false, extent_protocol::ALL_CACHED};
        }

    }


    return extent_protocol::OK;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid,
                       extent_protocol::attr &attr) {
    if (isdir(eid)) {
        return cl->call(extent_protocol::getattr, eid, attr);
    }
    decltype(cache_.begin()) iter;
    {
        std::unique_lock<std::mutex> m_(cache_mtx_);
        iter = cache_.find(eid);
        if (cache_.end() != iter && (iter->second.status & extent_protocol::ATTR_CACHED) ) {
            attr = iter->second.attr;
            return extent_protocol::OK;
        }
    }

    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::getattr, eid, attr);
    
    {
        std::unique_lock<std::mutex> m_(cache_mtx_);
        if (cache_.end() != iter) {
            iter->second.attr = attr;
            iter->second.status |= extent_protocol::ATTR_CACHED;
        } else {
            cache_[eid] = {eid, attr, std::string(), false, extent_protocol::ATTR_CACHED};
        }

    }

    return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf) {
    decltype(cache_.begin()) iter;
    std::unique_lock<std::mutex> m_(cache_mtx_);
    iter = cache_.find(eid);
    if (cache_.end() != iter && (iter->second.status == extent_protocol::ALL_CACHED)) {
        iter->second.buf = buf;
        iter->second.attr.mtime = iter->second.attr.atime = std::time(nullptr);
        iter->second.attr.size = buf.size();
        iter->second.modified = true;
        return extent_protocol::OK;
    } 

    extent_protocol::status ret = extent_protocol::OK;
    extent_protocol::attr attr;
    attr.atime = attr.mtime = attr.ctime = std::time(nullptr);
    attr.size = buf.size();
    cache_[eid] = {eid, attr, buf, true, extent_protocol::ALL_CACHED};
    return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid) {
    {
        std::unique_lock<std::mutex> m_(cache_mtx_);
        auto iter = cache_.find(eid);
        if (cache_.end() != iter) {
            cache_.erase(iter);
        }
    }
    extent_protocol::status ret = extent_protocol::OK;
    int r;
    ret = cl->call(extent_protocol::remove, eid, r);
    return ret;
}


extent_protocol::status extent_client::create(extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t id){
    int r;
    return cl->call(extent_protocol::create, pid, name, id, r);
}

extent_protocol::status extent_client::lookup(extent_protocol::extentid_t pid, std::string name,
                                              extent_protocol::extentid_t &id) {
    return  cl->call(extent_protocol::lookup, pid, name, id);
}
extent_protocol::status extent_client::readdir(extent_protocol::extentid_t pid, std::map<std::string, extent_protocol::extentid_t> &ents){
    return cl->call(extent_protocol::readdir, pid, ents);
}

extent_protocol::status extent_client::flush(extent_protocol::extentid_t id) {
    if (isdir(id) ) {
        return extent_protocol::OK;
    }

    extent_cache cache;
    {
        std::unique_lock<std::mutex> m_(cache_mtx_);
        auto iter = cache_.find(id);
        if (cache_.end() != iter) {
            cache = iter->second;
            cache_.erase(iter);
        } else {
            return extent_protocol::OK;
        }
    }

    if (cache.modified) {
        int r;
        cl->call(extent_protocol::flush, id, cache.buf, cache.attr, cache.status, r);

    }

    return extent_protocol::OK;
}
