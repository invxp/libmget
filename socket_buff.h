#pragma once

#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/system/error_code.hpp>
#include <boost/bind.hpp>

enum http_status
{
    HTTP_INIT=0,
    HTTP_DATA,
    HTTP_FINISH,
    HTTP_STOP,
    HTTP_ERROR
};

class socket_manage;
class socket_buff:public boost::enable_shared_from_this<socket_buff>
{
public:
    socket_buff(boost::asio::io_service& io,const boost::shared_ptr<socket_manage>& man,const std::string& file_name,const std::string& url,const unsigned int& time_out_sec=30);
    ~socket_buff();

public:
    void reset(const unsigned int& time_out_sec=30);
    void init();
private:
    void dead_line_timeout(const boost::system::error_code& ec);
    
public:
    boost::asio::ip::tcp::socket    socket_;
    std::size_t                     start_;
    std::size_t                     end_;
    unsigned int                    id_;
    unsigned int                    status_;
    boost::asio::streambuf          request_;
    boost::asio::streambuf          response_;
    std::string                     last_buff_;
    std::string						url_;
    std::string                     file_name_;
    bool                            can_multi_get_;
    boost::asio::deadline_timer     time_out_;
    unsigned int                    time_out_count_;
    unsigned int                    time_out_sec_;
    boost::shared_ptr<socket_manage>man_;
};