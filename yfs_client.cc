// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <random>
#include <cstring>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst): mt(std::random_device()()) {
    ec = new extent_client(extent_dst);
    lu = new lock_release(ec);
    lc = new lock_client_cache(lock_dst, lu);
}

yfs_client::~yfs_client() {
    delete lc;
    delete lu;
    delete ec;
}

yfs_client::inum
yfs_client::n2i(std::string n) {
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum) {
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum) {
    if (inum & 0x80000000)
        return true;
    return false;
}

bool
yfs_client::isdir(inum inum) {
    return !isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin) {
    int r = OK;
    lc_guard lc_(lc, inum);

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

    release:

    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din) {
    int r = OK;
    lc_guard lc_(lc, inum);

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

    release:
    return r;
}

yfs_client::status
yfs_client::create(inum parent, std::string name, inum &ino, bool is_dir) {
    ino = 0;
    lc_guard(lc, parent);
    auto ret = lookup(parent, name, ino);
    if (OK == ret){
        return EXIST;
    }

    extent_protocol::attr attr;

    for (int i = 0; i < 10; ++i){
        if (is_dir){
            ino = mt() & 0x7FFFFFFF;
        } else {
            ino = mt() | 0x80000000;
        }
        if (NOENT == ec->getattr(ino, attr)){
            return ec->create(parent, name, ino);
        }
    }
    return IOERR;
}

yfs_client::status
yfs_client::mkdir(inum parent, std::string name, inum &ino) {
    return create(parent, name, ino, true);
}

yfs_client::status
yfs_client::unlink(inum parent, std::string name){
    inum ino;
    lc_guard lc_(lc, parent);
    auto ret = lookup(parent, name, ino);
    if (OK != ret){
        return ret;
    }
    printf("UNLINK== his ino is %llu\n", ino);
    if (isdir(ino)){
        return IOERR;
    }
    return ec->remove(ino);
}

yfs_client::status
yfs_client::lookup(inum parent, std::string name, inum &ino) {
    return  ec->lookup(parent, name, ino);
}

yfs_client::status
yfs_client::readdir(inum parent, std::map<std::string, extent_protocol::extentid_t> &ents){
    return  ec->readdir(parent, ents);
}


yfs_client::status yfs_client::read(yfs_client::inum ino, std::size_t size, std::size_t off, std::string &buf) {
    std::string tmp;
    auto ret = ec->get(ino, tmp);
    if (extent_protocol::OK !=  ret){
        return ret;
    }
    buf = std::move(tmp.substr(off, size));
    return ret;
}

yfs_client::status yfs_client::write(yfs_client::inum ino, std::size_t size, std::size_t off, const char *cbuf) {
    lc_guard lc_(lc, ino);

    std::string tmp;
    auto ret = ec->get(ino, tmp);
    if (extent_protocol::OK != ret){
        return ret;
    }

    tmp.resize(std::max(tmp.size(), off + size), '\0');
    for (std::size_t i = off, j = 0; j < size; ++i,++j){
        tmp[i] = cbuf[j];
    }
    return ec->put(ino, tmp);
}

yfs_client::status yfs_client::setattr(inum ino, struct stat *st) {
    // Update mtime, atime
    lc_guard(lc, ino);
    std::string tmp;
    auto ret = ec->get(ino, tmp);
    if (OK != ret){
        return ret;
    }
    if (st->st_size < (int)tmp.size()){
        tmp = tmp.substr(0, st->st_size);
    } else {
        tmp += std::string('\0', st->st_size - tmp.size());
    }
    return ec->put(ino, tmp);
}
