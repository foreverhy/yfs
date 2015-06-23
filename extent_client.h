// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <map>
#include <mutex>
#include "extent_protocol.h"
#include "rpc.h"

class yfs_client;

class extent_client {
    rpcc *cl;

    struct extent_cache {
        extent_protocol::extentid_t id;
        extent_protocol::attr attr;
        std::string buf;
        bool modified;
        int status;
    };
    
    std::map<extent_protocol::extentid_t, extent_cache> cache_;
    std::mutex cache_mtx_;

  public:
    extent_client(std::string dst);

    extent_protocol::status get(extent_protocol::extentid_t eid,
                                std::string &buf);

    extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                    extent_protocol::attr &a);

    extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);

    extent_protocol::status remove(extent_protocol::extentid_t eid);

    extent_protocol::status flush(extent_protocol::extentid_t eid);
//    extent_protocol::status create(bool is_dir, extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t &id);
    extent_protocol::status create(extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t id);
    extent_protocol::status lookup(extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t &id);
    extent_protocol::status readdir(extent_protocol::extentid_t , std::map<std::string, extent_protocol::extentid_t> &);
};

#endif

