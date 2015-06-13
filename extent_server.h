// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>


#include <map>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <random>
#include "extent_protocol.h"
#include "yfs_client.h"

struct dir_ent{
    std::string name;
    extent_protocol::extentid_t eid;

    dir_ent(const std::string &nm, extent_protocol::extentid_t id): name(nm), eid(id){}
    dir_ent(dir_ent &&rhs): name(std::move(rhs.name)), eid(rhs.eid){}
};

struct extent_entry {
    extent_protocol::extentid_t eid;
    extent_protocol::extentid_t parent_id; // 0 for not set, 1 for '/'
    // @name is not essential, will be removed.
    std::string name;
    extent_protocol::attr attr;

    std::list<dir_ent> chd;
    std::string buf;
    int n_chd;

    extent_entry (const extent_entry &rhs):eid(rhs.eid), parent_id(rhs.parent_id), name(rhs.name), attr(rhs.attr), n_chd(0){}
    extent_entry (extent_entry &&rhs):eid(rhs.eid), parent_id(rhs.parent_id), name(std::move(rhs.name)), attr(rhs.attr), n_chd(0){}
    extent_entry& operator=(extent_entry &&rhs){
        if (&rhs != this){
            eid = rhs.eid;
            parent_id = rhs.parent_id;
            name = std::move(rhs.name);
            attr = rhs.attr;
            n_chd = rhs.n_chd;
        }
        return *this;
    }
    extent_entry () = default;
    extent_entry (extent_protocol::extentid_t id, extent_protocol::extentid_t pid, std::string nm, int sz = 0)
           : eid(id), parent_id(pid), name(nm), attr({0,0,0,0}), buf(""), n_chd(0){
        attr.atime = attr.ctime = attr.mtime = std::time(nullptr);
        attr.size = sz;
    }
};

class extent_server {
    std::mutex mtx_;
    std::map<extent_protocol::extentid_t, std::shared_ptr<extent_entry> > extents_;
    std::mt19937 rand_;


    bool isfile(extent_protocol::extentid_t id){
        return (id & 0x80000000) != 0;
    }
  public:
    extent_server();

    int put(extent_protocol::extentid_t id, std::string, int &);

    int get(extent_protocol::extentid_t id, std::string &);

    int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);

    int remove(extent_protocol::extentid_t id, int &);

    int flush(extent_protocol::extentid_t, std::string, extent_protocol::attr, int status, int &);


    int create(extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t id, int &);

    int lookup(extent_protocol::extentid_t pid, std::string name, extent_protocol::extentid_t &ret);

    int readdir(extent_protocol::extentid_t pid, std::map<std::string, extent_protocol::extentid_t> &ents);

};

#endif


