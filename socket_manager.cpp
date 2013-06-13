#include "socket_manager.h"

socket_manage::socket_manage(boost::asio::io_service& io,const std::string& url,const std::string& file_name,const unsigned int& segment,const unsigned int& save_per_sec):content_length_(0),file_name_(file_name),file_name_cache_(file_name+".mget"),file_name_info_(file_name+".cache"),bytes_writed_(0),url_(url),deadline_timer_(io,boost::posix_time::seconds(save_per_sec)),save_per_sec_(save_per_sec),segment_(segment),can_multi_get_(true)
{
	segment_=segment_==0?1:segment_;
}

socket_manage::~socket_manage()
{
    file_.close();
}

bool socket_manage::init()
{
    deadline_timer_.async_wait(boost::bind(&socket_manage::io_deadline_callback,this,boost::asio::placeholders::error));
    
    if (boost::filesystem::exists(file_name_))
        return false;

    boost::filesystem::exists(file_name_cache_)?file_.open(file_name_cache_.c_str(),std::ios::binary|std::ios::out|std::ios::_Nocreate):file_.open(file_name_cache_.c_str(),std::ios::binary|std::ios::out);

    return !file_.fail();
}

void socket_manage::release()
{
    boost::system::error_code ec;
    file_.close();
    deadline_timer_.cancel(ec);
}

bool socket_manage::write(const char* chr,std::size_t sz,long pos/* =0 */)
{
    file_.seekp(pos,std::ios::beg);
    file_.write(chr,sz);
    if (file_.fail())
        return false;
    else
        bytes_writed_+=sz;

    file_.flush();
    return !file_.fail();
}


bool socket_manage::load(boost::asio::io_service& io,const std::string& file_name)
{
    try
    {
        boost::property_tree::ptree root;
        boost::property_tree::read_json(file_name_info_,root);
        boost::property_tree::ptree child=root.get_child("cache");
        url_=root.get<std::string>("url");  
        segment_=child.size();
        bytes_writed_=root.get<std::size_t>("bytes-writed");
        content_length_=root.get<std::size_t>("content-length");
        BOOST_FOREACH(boost::property_tree::ptree::value_type& v,child)
        {
            boost::property_tree::ptree& p=v.second;
            boost::shared_ptr<socket_buff> buff(new socket_buff(io,shared_from_this(),file_name_,url_));
            buff->init();
            buff->id_=boost::lexical_cast<unsigned int>(v.first);
            buff->start_=p.get<std::size_t>("start");
            buff->end_=p.get<std::size_t>("end");
            buff->can_multi_get_=content_length_>0?true:false;
            buffs_.push_back(buff);
        }
        return true;
    }
    catch (...)
    {
        return false;
    }

}
void socket_manage::save()
{
    if (!can_multi_get_)
        return;

    boost::property_tree::ptree root;
    boost::property_tree::ptree tree;

    BOOST_FOREACH(boost::shared_ptr<socket_buff>& buff,buffs_)
    {
        boost::property_tree::ptree child;
        child.put("start",buff->start_);
        child.put("end",buff->end_);
        tree.push_back(std::make_pair(boost::lexical_cast<std::string>(buff->id_), child));
    }

    if (tree.empty())
    {
        boost::system::error_code ec;
        boost::filesystem::remove(file_name_info_,ec);
    }else
    {
        root.put("url",url_);
        root.put("content-length",content_length_);
        root.put("bytes-writed",bytes_writed_);

        root.push_back(std::make_pair("cache", tree));
        boost::property_tree::write_json(file_name_info_,root);
    }

}

void socket_manage::io_deadline_callback(const boost::system::error_code& ec)
{
    if (ec!=boost::asio::error::operation_aborted)
    {  
        mutex_.lock();
        save();
        mutex_.unlock();
         
        deadline_timer_.expires_from_now(boost::posix_time::seconds(save_per_sec_));
        deadline_timer_.async_wait(boost::bind(&socket_manage::io_deadline_callback,this,boost::asio::placeholders::error));
    }  
}