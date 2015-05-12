// the extent server implementation

#include "extent_server.h"
#include "rpc/marshall.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>

extent_server::extent_server() { }


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &) {
    // You fill this in for Lab 2.
    std::unique_lock<std::mutex> m_(mtx_);
    auto iter = extents_.find(id);
    if (iter == extents_.end()){
        // New entry
        extents_[id] = std::move(extent_entry(std::move(buf), std::time(nullptr)));
    } else {
        // Old entry, update name and mtime, atime
        iter->second.name_ = std::move(buf);
        iter->second.attr_.mtime = iter->second.attr_.atime = std::time(nullptr);
    }
    return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
    // You fill this in for Lab 2.
    std::unique_lock<std::mutex> m_(mtx_);
    auto iter = extents_.find(id);
    if (iter != extents_.end()){
        buf = iter->second.name_;
        iter->second.attr_.atime = std::time(nullptr);
        return extent_protocol::OK;
    }
    return extent_protocol::NOENT;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a) {
    // You fill this in for Lab 2.
    // You replace this with a real implementation. We send a phony response
    // for now because it's difficult to get FUSE to do anything (including
    // unmount) if getattr fails.
    std::unique_lock<std::mutex> m_(mtx_);
    auto it = extents_.find(id);
    if (it != extents_.end()){
        a = it->second.attr_;
        m_.unlock();
        return extent_protocol::OK;
    }
    return extent_protocol::NOENT;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
    // You fill this in for Lab 2.
    return extent_protocol::IOERR;
}

