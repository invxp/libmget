#pragma once

#include "socket_buff.h"

#include <vector>
#include <string>
#include <fstream>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#define BOOST_SPIRIT_THREADSAFE

class socket_manage:public boost::enable_shared_from_this<socket_manage>
{
public:
    socket_manage(boost::asio::io_service& io,const std::string& url,const std::string& file_name,const unsigned int& segment=10,const unsigned int& save_per_sec=60);
    ~socket_manage();

public:
    bool init();
    bool write(const char* chr,std::size_t sz,long pos=0);
    void save();
    bool load(boost::asio::io_service& io,const std::string& file_name);
    void release();
    
private:
    void io_deadline_callback(const boost::system::error_code& ec);
    
public:
    std::vector< boost::shared_ptr<socket_buff> >  buffs_;
    boost::shared_mutex                            mutex_;
    std::string                                    url_;
    std::string                                    file_name_;
    std::string                                    file_name_cache_;
    std::string                                    file_name_info_;
    std::size_t                                    content_length_;
    std::size_t                                    bytes_writed_;
    std::ofstream                                  file_;
    boost::asio::deadline_timer                    deadline_timer_;
    unsigned int                                   save_per_sec_;
    unsigned int                                   segment_;
    bool                                           can_multi_get_;
};