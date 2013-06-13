#include "libmget.h"

bool libmget::started_=false;

libmget::libmget(const callback& cb,bool console_mode):cb_(cb),resolver_(io_service_),console_(console_mode)
{

}

libmget::~libmget()
{
    
}

void libmget::heart_beat_callback(const boost::shared_ptr<socket_buff>& buff,bool finish,bool all_finish,bool err)
{
    boost::shared_lock<boost::shared_mutex> lock(socks_mutex_);
    if (console_)
        progress();
    cb_(buff,socks_,finish,all_finish,buff->status_==HTTP_ERROR?true:false);
}

bool libmget::get(const std::string& url,const std::string& file_name,const unsigned int& segment)
{

    boost::unique_lock<boost::shared_mutex> lock(socks_mutex_);
    std::map< std::string,boost::shared_ptr<socket_manage> >::iterator it;
    it=socks_.find(file_name);
    if (it!=socks_.end())
        return false;
        
    boost::shared_ptr<socket_manage> man(new socket_manage(io_service_,url,file_name,segment,60));
    if (!man->init())
        return false;

    socks_[file_name]=man;

    if (!man->load(io_service_,file_name))
    {
        boost::shared_ptr<socket_buff> buff(new socket_buff(io_service_,man,file_name,url));
        buff->init();
		man->buffs_.push_back(buff);
        connect_to_server(buff);
    }else
        start_segment(socks_[file_name]);

	if (!started_)
		boost::thread td(boost::bind(&io_thread,boost::ref(io_service_)));

    return true;
}

void libmget::start_segment(const boost::shared_ptr<socket_manage>& man)
{
    boost::shared_lock<boost::shared_mutex> lock(man->mutex_);

    BOOST_FOREACH(boost::shared_ptr<socket_buff> &buff,man->buffs_)
        connect_to_server(buff);
}


void libmget::connect_to_server(boost::shared_ptr<socket_buff>& buff)
{
	buff->status_=HTTP_INIT;
    std::string host,file;
	if (!http_split(buff->url_,host,file))
    {
        buff->status_=HTTP_ERROR;
		if (console_)
			progress();
		cb_(buff,socks_,false,false,true);
    }else
    {
		boost::asio::ip::tcp::resolver::query query(host,"http");

        resolver_.async_resolve(query,
            boost::bind(&libmget::handle_dns_resolved,this,
            boost::asio::placeholders::error, boost::asio::placeholders::iterator,buff));
    }
}

void libmget::handle_dns_resolved(const boost::system::error_code & ec, const boost::asio::ip::tcp::resolver::iterator &endpoint_iterator,boost::shared_ptr<socket_buff> buff)
{
	if (!ec)
		boost::asio::async_connect(buff->socket_, endpoint_iterator,
			boost::bind(&libmget::handle_connect_request, this,
            boost::asio::placeholders::error,boost::asio::placeholders::iterator,buff));
    else
        restart(buff);

}

void libmget::handle_connect_request(const boost::system::error_code& ec,const boost::asio::ip::tcp::resolver::iterator &endpoint_iterator,boost::shared_ptr<socket_buff> buff)
{
    if (!ec)
    {
        std::ostream request_stream(&buff->request_);
        
        std::string host,file;
        if (!http_split(buff->url_,host,file))
        {
            buff->status_=HTTP_ERROR;
            heart_beat_callback(buff);
        }
        else
        {
            request_stream << "GET " << file << " HTTP/1.1\r\n";  
            request_stream << "Host: " << host << "\r\n"; 
            request_stream << "Accept: */*\r\n";
            request_stream << "User-Agent: Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.0; Trident/4.0)\r\n";
            // means we have set range 
            if (buff->can_multi_get_)
                request_stream << "Range: bytes=" << buff->start_ << "-" << buff->end_ << "\r\n";

            buff->can_multi_get_?request_stream << "Connection: Keep-Alive\r\n\r\n":request_stream << "Connection: close\r\n\r\n"; 

            boost::asio::async_write(buff->socket_, buff->request_,
                boost::bind(&libmget::handle_write_request, this,
                boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,buff));
            
            buff->reset();
        }

    }
    else
        restart(buff);

}


void libmget::handle_write_request(const boost::system::error_code& ec,std::size_t writed,boost::shared_ptr<socket_buff> buff)
{
	if (!ec)
	{
		buff->request_.consume(writed);		
		if (buff->request_.size())			
			boost::asio::async_write(buff->socket_,
			buff->request_,
            boost::bind(&libmget::handle_write_request,this,boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred,buff));
		else
			boost::asio::async_read_until(buff->socket_, buff->response_, "\r\n\r\n",
			boost::bind(&libmget::handle_read_request,this,
            boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred,buff));
        
        buff->reset();

	}else
        restart(buff);


}

void libmget::handle_read_request(const boost::system::error_code& ec,std::size_t readed,boost::shared_ptr<socket_buff> buff)
{
    if (!ec)
        process_request(buff,readed);
    else if (ec==boost::asio::error::eof)
    {
        // server didn't support <range> finish data
        if (buff->end_==std::numeric_limits<std::size_t>::max() && readed==0)
            buff_finish(buff);
        // continue process the last_buffer
        else if (readed)
            process_request(buff,readed);
        else
        {
            buff->status_=HTTP_ERROR;
            heart_beat_callback(buff);
        }
    }
    else
        restart(buff);

}

void libmget::process_request(boost::shared_ptr<socket_buff>& buff,std::size_t condition)
{
    if (buff->status_==HTTP_INIT)
    {
        std::istream is(&buff->response_);
        is.unsetf(std::ios_base::skipws);
        std::string request;
        request.append(std::istream_iterator<char>(is), std::istream_iterator<char>());

        buff->last_buff_=request.substr(condition);

        unsigned int http_code=parse_http_request_code(request);

        if ( (http_code==0) || (http_code>=400) )
        {
            buff->status_=HTTP_ERROR;
            buff->socket_.close();
            heart_beat_callback(buff);
            return;
        }

        if ( (http_code >= 300) && (http_code<400) )
        {
            // relocation
            if (!check_relocate(request,buff))
            {
                buff->status_=HTTP_ERROR;
                buff->socket_.close();
                heart_beat_callback(buff);
            }
            return;
        }

        if ( (http_code >= 200) && (http_code<300) )
        {
            std::size_t content_length=parse_http_content_length(request);

            // server doesn't support range bytes
            if ( (content_length==0) || (content_length==std::numeric_limits<std::size_t>::max()) )
            {
                buff->man_->can_multi_get_=false;
                buff->can_multi_get_=false;
                buff->status_=HTTP_DATA;
                buff->end_=std::numeric_limits<std::size_t>::max();
                process_request(buff,buff->last_buff_.length());
                return;
            }

            // we request Range 0-200,but server returns content-length 201,fixed
            // and multi get will return to recv data
            if (buff->can_multi_get_)
            {
                size_t len=buff->end_-buff->start_;
                buff->end_+=content_length-(buff->end_-buff->start_);
                buff->status_=HTTP_DATA;
                process_request(buff,buff->last_buff_.length());
            }
            else
            {
                // yeah,server support range! we create multi thread to start
                std::string file=buff->file_name_;
                create_muliti_socket_buff(buff,content_length);
                start_segment(socks_[file]);
            }
        }

    }
    else if (buff->status_==HTTP_DATA)
    {
        std::istream is(&buff->response_);
        is.unsetf(std::ios_base::skipws);
        std::string data(buff->last_buff_);
        data.append(std::istream_iterator<char>(is),std::istream_iterator<char>());
        buff->start_+=data.length();
        buff->last_buff_.clear();
        if (!buff->man_->write(data.c_str(),data.length(),buff->start_-data.length()))
        {
            buff->status_=HTTP_ERROR;
            buff->socket_.close();
            heart_beat_callback(buff);
            return;
        }

        if (buff->start_==buff->end_)
            buff_finish(buff);
        else
        {

            heart_beat_callback(buff);
                                    
            boost::asio::async_read(buff->socket_, buff->response_,boost::asio::transfer_at_least(1),
                boost::bind(&libmget::handle_read_request, this,
                boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred,buff));

            buff->reset();

        }

    }
}

void libmget::create_muliti_socket_buff(const boost::shared_ptr<socket_buff> buff,const std::size_t content_length)
{

    boost::shared_ptr<socket_manage>& man=socks_[buff->file_name_];
    std::size_t piece=content_length/man->segment_;
    man->url_=buff->url_;
    man->content_length_=content_length;

    for (unsigned int i=0;i<man->segment_;i++)
    {
        boost::shared_ptr<socket_buff> multi;
        if (i==0)
            multi=buff;
        else
        {
            multi.reset(new socket_buff(io_service_,man,man->file_name_,man->url_));
            multi->init();
            man->buffs_.push_back(multi);
        }

        multi->can_multi_get_=true;
        multi->id_=i;
        multi->start_=i==0?0:(piece*i)+i;

        if ((i+1)==man->segment_)
            multi->end_=content_length;
        else
            multi->end_=i==0?piece:multi->start_+piece;

    }

    // save the profile
    man->save();
    
}

bool libmget::http_split(const std::string& url,std::string& host,std::string& file)
{
    std::string temp=url;
    size_t pos=0;
    boost::to_lower<std::string>(temp);
    if (temp.substr(0,7)=="http://")
        temp=url.substr(7);
    pos=temp.find("/")+1;
    if (!pos)
        return false;
   
    host=temp.substr(0,pos-1);
    file=temp.substr(pos-1);
    return true;
}

void libmget::io_thread(boost::asio::io_service& io)
{
	started_=true;
    boost::system::error_code ec;
    io.run(ec);
	started_=false;
}

unsigned int libmget::parse_http_request_code(const std::string& request)
{
    std::string http_req;
    size_t pos=request.find("\r\n")+1;
    if (!pos) 
        return 0;
    http_req=request.substr(0,pos-1);

    std::vector<std::string> status_code;
    boost::split(status_code,http_req,boost::algorithm::is_any_of<std::string>(" "),boost::algorithm::token_compress_on);

    unsigned int request_code=0;

    if (status_code.size()>=3)
    {
        try
        {
            request_code=boost::lexical_cast<unsigned int>(status_code[1]);
        }
        catch(...)
        {
            return 0;
        }
    }
    
    return request_code;
}

std::size_t libmget::parse_http_content_length(const std::string& request)
{

    std::string content_length=request;
    boost::to_lower<std::string>(content_length);
    size_t pos=content_length.find("content-length: ")+1;

    if (!pos)
        return 0;

    content_length=request.substr(pos+15);
    pos=content_length.find("\r\n")+1;

    if (!pos)
        return 0;

    content_length=content_length.substr(0,pos-1);

    return boost::lexical_cast<std::size_t>(content_length);

}

bool libmget::check_relocate(const std::string& request,boost::shared_ptr<socket_buff>& buff)
{
    //relocate HTTP/306,307 and more
    std::string location=request;
    boost::to_lower<std::string>(location);

    size_t pos=location.find("location: ")+1;
    if (!pos)
        return false;

    location=request.substr(pos+9);
    pos=location.find("\r\n")+1;

    if (!pos)
        return false;

    location=location.substr(0,pos-1);
	buff->url_=location;
    buff->socket_.close();

    connect_to_server(buff);

    return true;
}

void libmget::buff_finish(boost::shared_ptr<socket_buff>& buff)
{
    buff->status_=HTTP_FINISH;
    if (man_remove(buff))
        socks_remove(buff);
}

bool libmget::man_remove(const boost::shared_ptr<socket_buff>& buff)
{
    boost::shared_ptr<socket_manage>& man=buff->man_;
    boost::unique_lock<boost::shared_mutex> lock(man->mutex_);
    std::vector< boost::shared_ptr<socket_buff> >::iterator it=man->buffs_.begin();
    for (it;it!=man->buffs_.end();it++)
    {
        if (*it==buff)
        {
            man->buffs_.erase(it);
            if (man->buffs_.empty())
            {
                man->release();
                boost::system::error_code ec;
                boost::filesystem::rename(man->file_name_cache_,man->file_name_,ec);
                boost::filesystem::remove(man->file_name_info_,ec);
                return true;
            }else
                man->save();
            return false;
        }
    }
    return false;
}

void libmget::socks_remove(const boost::shared_ptr<socket_buff>& buff)
{
    boost::shared_lock<boost::shared_mutex> lock(socks_mutex_);
    socks_.erase(buff->man_->file_name_);
    lock.unlock();
    heart_beat_callback(buff,true,socks_.empty()?true:false,false);
    
}
void libmget::restart(boost::shared_ptr<socket_buff>& buff)
{
    if (buff->status_==HTTP_STOP)
        return;

    buff->time_out_count_++;

    if (buff->time_out_count_<5)
        connect_to_server(buff);
    else
        buff->status_=HTTP_ERROR;

    heart_beat_callback(buff);
}

void libmget::stop(const std::string& file_name)
{
    boost::shared_lock<boost::shared_mutex> lock(socks_mutex_);
    std::map< std::string,boost::shared_ptr<socket_manage> >::iterator it;
    it=socks_.find(file_name);
    if (it==socks_.end())
        return;
    lock.unlock();
    
    boost::shared_ptr<socket_manage>& man=it->second;
    boost::unique_lock<boost::shared_mutex> buff_lock(man->mutex_);
    BOOST_FOREACH (boost::shared_ptr<socket_buff> &buff,man->buffs_)
    {
        buff->status_=HTTP_STOP;
        buff->socket_.close();
    }
    man->save();
}

void libmget::progress()
{
	
	boost::lock_guard<boost::shared_mutex> lk(progress_mutex_);

    system("cls");

    BOOST_FOREACH (const_map& sock,socks_)
    {
        boost::shared_ptr<socket_manage> man=sock.second;
        float per=0;
        if (man->content_length_==0)
            per=-1;
        else
            per=((float)man->bytes_writed_/(float)man->content_length_)*100.00;

		if (per>100)
			per=100;

        std::cout << std::setprecision(3) << man->file_name_ <<" progress: " << per << std::endl;
    }

}