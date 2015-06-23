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
#include <fstream>

extent_server::extent_server(): rand_(std::random_device()()){
    extents_[1] = std::make_shared<extent_entry>(1, 0, "", 0);
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &ret) {
    // You fill this in for Lab 2.
    printf("=== PUT for %llu: %s\n", id, buf.data());
    std::unique_lock<std::mutex> m_(mtx_);
    auto iter = extents_.find(id);
    if (iter != extents_.end()){
        // update name and mtime, atime, size
        iter->second->buf = buf;
        iter->second->attr.mtime = iter->second->attr.atime = std::time(nullptr);
        iter->second->attr.size = iter->second->buf.size();
        ret = iter->second->buf.size();
        return extent_protocol::OK;
    }
    return extent_protocol::NOENT;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf) {
    // You fill this in for Lab 2.
    printf("=== GET for %llu \n", id);
    std::unique_lock<std::mutex> m_(mtx_);
    auto iter = extents_.find(id);
    if (iter != extents_.end()){
        buf = iter->second->buf;
        iter->second->attr.atime = std::time(nullptr);
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
    auto iter = extents_.find(id);
    if (iter != extents_.end()){
        a = iter->second->attr;
        return extent_protocol::OK;
    }
    return extent_protocol::NOENT;
}

int extent_server::remove(extent_protocol::extentid_t id, int &) {
    // You fill this in for Lab 2.
    std::unique_lock<std::mutex> m_(mtx_);
    auto it = extents_.find(id);
    if (it != extents_.end()){
        auto piter = extents_.find(it->second->parent_id);
        std::list<dir_ent> &chdlst = piter->second->chd;
        for (auto lstiter = chdlst.begin(); lstiter != chdlst.end(); ++lstiter){
            if (lstiter->eid == id){
                chdlst.erase(lstiter);
                break;
            }
        }
        piter->second->attr.mtime = piter->second->attr.ctime = std::time(nullptr);
        extents_.erase(it);
        return extent_protocol::OK;
    }
    return extent_protocol::NOENT;
}
int extent_server::create(extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t id, int &){
    std::unique_lock<std::mutex> m_(mtx_);

    printf("CREATE:: %s, %llu\n", name.data(), id);

    auto piter = extents_.find(pid);
    if (piter == extents_.end() || isfile(pid)){
        return extent_protocol::IOERR;
    }

    std::list<dir_ent> &chdlst = piter->second->chd;
    auto lstiter = chdlst.cbegin();
    for (; lstiter != chdlst.cend(); ++lstiter){
        if (lstiter->name == name){
            return extent_protocol::EXIST;
        } else if (lstiter->name > name){
            break;
        }
    }
    chdlst.insert(lstiter, dir_ent(name, id));
    piter->second->attr.mtime = piter->second->attr.ctime = std::time(nullptr);
    extents_[id] = std::make_shared<extent_entry>(id, pid, name, 0);
    return extent_protocol::OK;
}

int extent_server::lookup( extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t &ret) {

    ret = 0;
    std::unique_lock<std::mutex> m_(mtx_);
    auto piter = extents_.find(pid);
    if (piter == extents_.end()){
        return extent_protocol::IOERR;
    }
    std::list<dir_ent> &chdlst = piter->second->chd;
    auto lstiter = chdlst.cbegin();
    for (; lstiter != chdlst.cend(); ++lstiter){
        if (lstiter->name == name){
            ret = lstiter->eid;
            return extent_protocol::OK;
        } else if (lstiter->name > name){
            return extent_protocol::NOENT;
        }
    }
    return extent_protocol::NOENT;
}

int extent_server::readdir(extent_protocol::extentid_t pid, std::map<std::string, extent_protocol::extentid_t> &ents){
    std::unique_lock<std::mutex> m_(mtx_);
    auto piter = extents_.find(pid);
    if (piter == extents_.end() || isfile(pid)){
        return extent_protocol::NOENT;
    }

    std::list<dir_ent> &chdlst = piter->second->chd;
    for (auto &item : chdlst){
        ents[item.name] = item.eid;
    }

    return extent_protocol::OK;
}

int extent_server::flush(extent_protocol::extentid_t id, std::string buff, extent_protocol::attr attr, int status, int &) {
    std::unique_lock<std::mutex> m_(mtx_);
    auto iter = extents_.find(id);
    if (iter != extents_.end()){
        // update name and mtime, atime, size
        if (status & extent_protocol::BUF_CACHED) {
            iter->second->buf = buff;
        }
        if (status & extent_protocol::ATTR_CACHED) {
            iter->second->attr = attr;
        }
        return extent_protocol::OK;
    }
    return extent_protocol::NOENT;
}
