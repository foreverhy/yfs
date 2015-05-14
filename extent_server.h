// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <unordered_map>
#include <mutex>
#include <memory>
#include "extent_protocol.h"


struct extent_entry {
    extent_protocol::extentid_t eid;
    extent_protocol::extentid_t parent_id; // 0 for not set, 1 for '/'
    std::string name;
    extent_protocol::attr attr;

    extent_entry (const extent_entry &rhs):eid(rhs.eid), parent_id(rhs.parent_id), name(rhs.name), attr(rhs.attr){}
    extent_entry (extent_entry &&rhs):eid(rhs.eid), parent_id(rhs.parent_id), name(std::move(rhs.name)), attr(rhs.attr){}
    extent_entry& operator=(extent_entry &&rhs){
        if (&rhs != this){
            eid = rhs.eid;
            parent_id = rhs.parent_id;
            name = std::move(rhs.name);
            attr = rhs.attr;
        }
        return *this;
    }
    extent_entry () = default;
    extent_entry (extent_protocol::extentid_t id, extent_protocol::extentid_t pid, std::string nm)
           : eid(id), parent_id(pid), name(nm), attr({0,0,0,0}){
        attr.atime = attr.ctime = attr.mtime = std::time(nullptr);
    }
};

struct dir_entry: public extent_entry{
    std::unordered_map<std::string, extent_protocol::extentid_t> chd;
    dir_entry(extent_protocol::extentid_t id, extent_protocol::extentid_t pid, std::string nm)
           : extent_entry(id, pid, nm){}
};

struct file_entry: public extent_entry{
    file_entry(extent_protocol::extentid_t id, extent_protocol::extentid_t pid, std::string nm,int sz)
    : extent_entry(id, pid, nm){
        attr.size = sz;
    }
};

class extent_server {
    std::mutex mtx_;
    std::map<extent_protocol::extentid_t, std::shared_ptr<extent_entry> > extents_;

    bool isfile(extent_protocol::extentid_t id){
        return (id & 0x80000000) != 0;
    }
  public:
    extent_server();

    int put(extent_protocol::extentid_t id, std::string, int &);

    int get(extent_protocol::extentid_t id, std::string &);

    int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);

    int remove(extent_protocol::extentid_t id, int &);
};

#endif 







