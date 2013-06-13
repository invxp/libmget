#include "libmget.h"

void mget_cb(const boost::shared_ptr<socket_buff>& buff,const socket_map& socks,bool finish,bool all_finish,bool is_err)
{

}

int main(int argc,char** argv)
{
	if (argc<=2)
	{
		std::cout <<"ex:libmget.exe http://www.google.com.hk/index.html c:\\test.html" << std::endl;
		system("pause");
		return -1;
	}

	libmget lib(mget_cb,true);
	lib.get(argv[1],argv[2]);
	char buff[256]={0};
	gets(buff);
	return 0;
}