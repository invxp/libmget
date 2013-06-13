/*
 * Copyright (C) 2013 InvXp <invidentssc@hotmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/

#define LIBMGET_VERSION "0.1b"

#pragma once

#ifndef LIB_MGET
#define LIB_MGET

#include "socket_manager.h"
#include "socket_buff.h"

#include <boost/function.hpp>
#include <boost/algorithm/string.hpp>

typedef std::map< std::string, boost::shared_ptr<socket_manage> > socket_map;
typedef const std::pair< std::string, boost::shared_ptr<socket_manage> > const_map;

typedef boost::function<void(const boost::shared_ptr<socket_buff>& buff,const socket_map& socks,bool finish=false,bool all_finish=false,bool err=false)> callback;

class libmget
{
public:
    libmget(const callback& cb,bool console_mode=true);
    ~libmget();
public:
    bool get(const std::string& url,const std::string& file_name,const unsigned int& segment=10);
    void stop(const std::string& file_name);
    void remove(const std::string& file_name);

public:
    void progress();

private:
    void handle_dns_resolved(const boost::system::error_code & ec, const boost::asio::ip::tcp::resolver::iterator &endpoint_iterator,boost::shared_ptr<socket_buff> buff);
    void handle_connect_request(const boost::system::error_code& ec,const boost::asio::ip::tcp::resolver::iterator &endpoint_iterator,boost::shared_ptr<socket_buff> buff);
    void handle_read_request(const boost::system::error_code& ec, std::size_t readed,boost::shared_ptr<socket_buff> buff);
    void handle_write_request(const boost::system::error_code& ec, std::size_t writed,boost::shared_ptr<socket_buff> buff);

private:
    void buff_finish(boost::shared_ptr<socket_buff>& buff);
    bool man_remove(const boost::shared_ptr<socket_buff>& buff);
    void socks_remove(const boost::shared_ptr<socket_buff>& buff);
    void restart(boost::shared_ptr<socket_buff>& buff);

private:
    void start_segment(const boost::shared_ptr<socket_manage>& man);
    void connect_to_server(boost::shared_ptr<socket_buff>& buff);

private:
	bool            http_split(const std::string& url,std::string& host,std::string& file);

    void            process_request(boost::shared_ptr<socket_buff>& buff,std::size_t condition);
    void            create_muliti_socket_buff(const boost::shared_ptr<socket_buff> buff,const std::size_t content_length);

    unsigned int    parse_http_request_code(const std::string& request);
    std::size_t     parse_http_content_length(const std::string& request);

    bool            check_relocate(const std::string& request,boost::shared_ptr<socket_buff>& buff);

protected:
    static void     io_thread(boost::asio::io_service& io);
    
    void            heart_beat_callback(const boost::shared_ptr<socket_buff>& buff,bool finish=false,bool all_finish=false,bool err=false);
    
private:
    boost::asio::io_service                                     io_service_;
    boost::asio::ip::tcp::resolver                              resolver_;
    bool                                                        console_;
    socket_map                                                  socks_;  
    boost::shared_mutex                                         socks_mutex_;
	boost::shared_mutex											progress_mutex_;
    callback                                                    cb_;
	static	bool												started_;

};
#endif