// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include <mutex>
#include "extent_protocol.h"

class extent_server {
    struct extent_entry {
        std::string name_;
        extent_protocol::attr attr_;
        extent_entry (const extent_entry &rhs) = delete;
        extent_entry (std::string &&name): name_(std::move(name)), attr_({0,0,0,0}){}
        extent_entry (extent_entry &&rhs): name_(std::move(rhs.name_)), attr_(rhs.attr_){}
        extent_entry& operator=(extent_entry &&rhs){
            if (&rhs != this){
                name_ = std::move(rhs.name_);
                attr_ = rhs.attr_;
            }
            return *this;
        }
        extent_entry (std::string &&name, unsigned int t): name_(std::move(name)), attr_({t,t,t,t}){}
        extent_entry (){}
    };
    std::mutex mtx_;
    std::map<extent_protocol::extentid_t, extent_entry> extents_;

  public:
    extent_server();

    int put(extent_protocol::extentid_t id, std::string, int &);

    int get(extent_protocol::extentid_t id, std::string &);

    int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);

    int remove(extent_protocol::extentid_t id, int &);
};

#endif 







