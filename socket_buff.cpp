#include "socket_buff.h"

socket_buff::socket_buff(boost::asio::io_service& io,const boost::shared_ptr<socket_manage>& man,const std::string& file_name,const std::string& url,const unsigned int& time_out_sec):socket_(io),start_(0),end_(0),status_(HTTP_INIT),id_(0),can_multi_get_(false),time_out_count_(0),time_out_(io,boost::posix_time::seconds(time_out_sec)),man_(man),time_out_sec_(time_out_sec),url_(url),file_name_(file_name)
{
    response_.prepare(512*1024);
}

socket_buff::~socket_buff()
{
    boost::system::error_code ec;
    socket_.close(ec);
}

void socket_buff::init()
{
    time_out_.async_wait(boost::bind(&socket_buff::dead_line_timeout,shared_from_this(),boost::asio::placeholders::error));
}

void socket_buff::reset(const unsigned int& time_out_sec)
{
    time_out_count_=0;
    time_out_sec_=time_out_sec;
    time_out_.cancel();
}

void socket_buff::dead_line_timeout(const boost::system::error_code& ec)
{
    if ( (status_==HTTP_FINISH) || (status_==HTTP_STOP) )
        return;

    if (ec!=boost::asio::error::operation_aborted)
    {    
        socket_.close();
        return;
    }
        
    time_out_.expires_from_now(boost::posix_time::seconds(time_out_sec_));
    time_out_.async_wait(boost::bind(&socket_buff::dead_line_timeout,shared_from_this(),boost::asio::placeholders::error));        

}