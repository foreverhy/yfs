// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

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
    //decltype(cache_.begin()) iter;
    {
        std::unique_lock<std::mutex> m_(cache_mtx_);
        auto iter = cache_.find(eid);
        if (cache_.end() != iter) {
            buf = iter->second.buf;
            return extent_protocol::OK;
        }
    }

    extent_protocol::attr attr;
    cl->call(extent_protocol::get, eid, buf);
    getattr(eid, attr);
    attr.atime = std::time(nullptr);

    std::unique_lock<std::mutex> m_(cache_mtx_);
    cache_[eid] = {eid, attr, buf, false};

    return extent_protocol::OK;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid,
                       extent_protocol::attr &attr) {
    {
        std::unique_lock<std::mutex> m_(cache_mtx_);
        auto iter = cache_.find(eid);
        if (cache_.end() != iter) {
            attr = iter->second.attr;
            return extent_protocol::OK;
        }
    }

    extent_protocol::status ret = extent_protocol::OK;
    ret = cl->call(extent_protocol::getattr, eid, attr);
    return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf) {
    {
        std::unique_lock<std::mutex> m_(cache_mtx_);
        auto iter = cache_.find(eid);
        if (cache_.end() != iter) {
            iter->second.buf = buf;
            iter->second.attr.mtime = iter->second.attr.atime = std::time(nullptr);
            iter->second.attr.size = buf.size();
            return extent_protocol::OK;
        }
    }

    extent_protocol::status ret = extent_protocol::OK;
    int r;
    ret = cl->call(extent_protocol::put, eid, buf, r);
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
