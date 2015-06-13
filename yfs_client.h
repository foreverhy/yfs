#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include "extent_client.h"
#include "raii.h"
#include "lock_client_cache.h"
#include <vector>
#include <random>

#include "lock_protocol.h"
#include "lock_client.h"


class yfs_client {
    extent_client *ec;
    lock_release_user *lu;
    lock_client_cache *lc;
    std::mt19937 mt;

  public:

    typedef unsigned long long inum;
    enum xxstatus {
        OK, RPCERR, NOENT, IOERR, EXIST
    };
    typedef int status;

    struct fileinfo {
        unsigned long long size;
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };
    struct dirinfo {
        unsigned long atime;
        unsigned long mtime;
        unsigned long ctime;
    };
    struct dirent {
        std::string name;
        yfs_client::inum inum;
    };

  private:
    static std::string filename(inum);

    static inum n2i(std::string);

  public:

    yfs_client(std::string, std::string);
    ~yfs_client();

    bool isfile(inum);

    bool isdir(inum);

    int getfile(inum, fileinfo &);

    int getdir(inum, dirinfo &);

    yfs_client::status create(inum parent, std::string name, inum &id, bool is_dir = false);
    yfs_client::status mkdir(inum parent, std::string name, inum &id);
    yfs_client::status unlink(inum parent, std::string name);
    yfs_client::status lookup(inum parent, std::string name, inum &id);
    yfs_client::status readdir(inum parent, std::map<std::string, extent_protocol::extentid_t> &);
    yfs_client::status read(inum ino, std::size_t size, std::size_t off, std::string &buf);
    yfs_client::status write(inum ino, std::size_t size, std::size_t off, const char *buf);
    yfs_client::status setattr(inum ino, struct stat *st);
};

#endif

