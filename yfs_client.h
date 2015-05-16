#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include "extent_client.h"
#include "raii.h"
#include "lock_client.h"
#include <vector>
#include <random>


class yfs_client {
    extent_client *ec;
    lock_client *lc;
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

    bool isfile(inum);

    bool isdir(inum);

    int getfile(inum, fileinfo &);

    int getdir(inum, dirinfo &);

    yfs_client::status create(inum parent, std::string name, inum &id);
    yfs_client::status lookup(inum parent, std::string name, inum &id);
    yfs_client::status readdir(inum parent, std::map<std::string, extent_protocol::extentid_t> &);
};

#endif
