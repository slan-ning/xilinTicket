#include "StdAfx.h"
#include "HttpClient.h"
#include <iostream>

#pragma comment( lib, "libeay32.lib" )
#pragma comment( lib, "ssleay32.lib" )

CHttpClient::CHttpClient(IoCallBack httpback,boost::asio::io_service& io_service)
	:socket_(io_service),
	mHttpBack(httpback),
	resolver_(io_service),
	ctx(boost::asio::ssl::context::sslv23),
	ssl_sock(socket_,ctx),
	protocol_(0),
	deadline_(io_service)
{
	nTimeOut=10000;
	bStop=true;
	m_body=NULL;
	m_bodyLen=0;
	m_respone=NULL;
}

CHttpClient::CHttpClient(boost::asio::io_service& io_service)
	:socket_(io_service),
	resolver_(io_service),
	ctx(boost::asio::ssl::context::sslv23),
	ssl_sock(socket_,ctx),
	protocol_(0),
	deadline_(io_service)
{
	nTimeOut=6000;
	bStop=true;
	m_body=NULL;
	m_bodyLen=0;
	m_respone=NULL;
}

CHttpClient::~CHttpClient(void)
{
	if(m_body!=NULL)
	{
		delete m_body;
	}

}


void CHttpClient::Send(int protocol, std::string Host, std::string port, char* body,size_t bodyLen)
{
	tcp::resolver::query query(Host,port);

	if (deadline_.expires_from_now(boost::posix_time::seconds(nTimeOut))>=0)
		deadline_.async_wait(boost::bind(&CHttpClient::check_deadline, this,boost::asio::placeholders::error));


	//解析域名。
	resolver_.async_resolve(query,boost::bind(&CHttpClient::handle_resolver,this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::iterator));
	//保存发送的包体
	m_body=body;
	m_bodyLen=bodyLen;

	//如果协议是ssl，就进行认证
	if(protocol==1)
	{
		protocol_=1;
		ssl_sock.set_verify_mode(boost::asio::ssl::verify_peer);
		ssl_sock.set_verify_callback(boost::bind(&CHttpClient::verify_certificate,this,_1,_2));


	}

}

int CHttpClient::SynSend(int protocol, std::string Host, std::string port, char* body,size_t bodyLen)
{
	tcp::resolver::query query(Host,port);

	//解析域名。
	tcp::resolver::iterator endpoint_iterator =resolver_.resolve(query);
	//保存发送的包体
	m_body=body;
	m_bodyLen=bodyLen;

	try{
		boost::asio::connect(socket_,endpoint_iterator);

		//如果协议是ssl，就进行认证
		if(protocol==1){
			protocol_=1;
			ssl_sock.set_verify_mode(boost::asio::ssl::verify_peer);
			ssl_sock.set_verify_callback(boost::bind(&CHttpClient::verify_certificate,this,_1,_2));
			ssl_sock.handshake(boost::asio::ssl::stream_base::client);
			boost::asio::write(ssl_sock,boost::asio::buffer(m_body,m_bodyLen));
		}else{
			boost::asio::write(socket_,boost::asio::buffer(m_body,m_bodyLen));
		}

		this->readBody();

	}
	catch(const boost::system::error_code& ex)
	{
		this->m_header=ex.message();
		return ex.value();
	}

	return 0;

}

void CHttpClient::readBody()
{
	int headSize;
	if(protocol_==1){
		headSize=boost::asio::read_until(ssl_sock,respone_,"\r\n\r\n");
	}else{
		headSize=boost::asio::read_until(socket_,respone_,"\r\n\r\n");
	}
	nHeaderLen=headSize;
	std::istream response_stream(&respone_);
	response_stream.unsetf(std::ios_base::skipws);//asio::streambuf 转换成istream 并且忽略空格

	//将数据流追加到header里
	int readSize=respone_.size();

	char * head=new char[headSize+1];
	ZeroMemory(head,headSize+1);
	response_stream.read(head,headSize);
	m_header=head;
	delete head;

	int rdContentSize=readSize-m_header.size();

	char * cont=NULL;
	//获取httpContent的长度
	if(m_header.find("Content-Length")!=std::string::npos)
	{
		std::string len=m_header.substr(m_header.find("Content-Length: ")+16);
		len=len.substr(0,len.find_first_of("\r"));
		nContentLen=atoi(len.c_str());

		if(protocol_==1){
			boost::asio::read(ssl_sock,respone_,boost::asio::transfer_at_least(nContentLen-rdContentSize));
		}else{
			boost::asio::read(socket_,respone_,boost::asio::transfer_at_least(nContentLen-rdContentSize));
		}

		nContentLen=respone_.size();
		cont=new char[nContentLen+1]; //此处申请了内存，注意释放。
		ZeroMemory(cont+nContentLen,1);
		response_stream.read(cont,nContentLen);

	}
	else if(m_header.find("Transfer-Encoding: chunked")!=std::string::npos)
	{
		while (true)
		{
			int contSize=0;
			if(protocol_==1){
				contSize=boost::asio::read_until(ssl_sock,respone_,"\r\n");
			}else{
				contSize=boost::asio::read_until(socket_,respone_,"\r\n");
			}

			int readLen=respone_.size()-contSize;

			char *chunkStr=new char[contSize]; //此处申请了内存，注意释放。
			response_stream.read(chunkStr,contSize);
			ZeroMemory(chunkStr+contSize-2,2);
			long nextReadSize=strtol(chunkStr,NULL,16);
			if(nextReadSize==0) break;
			delete chunkStr;

			char * htmlBuf=new char[nextReadSize+2];

			if(nextReadSize>readLen){
				if(protocol_==1){
					boost::asio::read(ssl_sock,respone_,boost::asio::transfer_at_least(nextReadSize-readLen+2));
				}else{
					boost::asio::read(socket_,respone_,boost::asio::transfer_at_least(nextReadSize-readLen+2));
				}
			}

			response_stream.read(htmlBuf,nextReadSize+2);

			if(cont==NULL){
				cont=htmlBuf;
				ZeroMemory(htmlBuf+nextReadSize,2);
				nContentLen=nextReadSize;
			}else{
				char * newCont=new char[nContentLen+nextReadSize+1];
				ZeroMemory(newCont+nContentLen+nextReadSize,1);
				memcpy(newCont,cont,nContentLen);
				memcpy(newCont+nContentLen,htmlBuf,nextReadSize);
				delete  cont;
				delete  htmlBuf;

				cont=newCont;
				nContentLen+=nextReadSize;
			}
		}

	}


	this->m_resLen=nContentLen;
	this->m_respone=cont;

}




void CHttpClient::handle_resolver(boost::system::error_code err, tcp::resolver::iterator endpoint_iterator)
{
	if(!err)
	{
		if (deadline_.expires_from_now(boost::posix_time::seconds(nTimeOut))>=0)
			deadline_.async_wait(boost::bind(&CHttpClient::check_deadline, this,boost::asio::placeholders::error));

		boost::asio::async_connect(socket_,endpoint_iterator,
			boost::bind(&CHttpClient::handle_connect,this,boost::asio::placeholders::error));
	}
	else
	{

		mHttpBack(this,NULL,"RESOLVERERR",0);
	}
}


bool CHttpClient::verify_certificate(bool preverified, boost::asio::ssl::verify_context& ctx)
{
	return true;
}


void CHttpClient::handle_connect(boost::system::error_code err)
{
	if(!err)
	{
		if (deadline_.expires_from_now(boost::posix_time::seconds(nTimeOut))>=0)
			deadline_.async_wait(boost::bind(&CHttpClient::check_deadline, this,boost::asio::placeholders::error));

		if(protocol_==1)
		{
			ssl_sock.async_handshake(boost::asio::ssl::stream_base::client,
				boost::bind(&CHttpClient::handle_handshake,this,boost::asio::placeholders::error));
		}
		else
		{
			boost::asio::async_write(socket_,boost::asio::buffer(m_body,m_bodyLen),
				boost::bind(&CHttpClient::handle_write,this,boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
		}
	}
	else
	{
		mHttpBack(this,NULL,"CONNECTERR",0);
	}
}

//ssl握手，握手后才能发数据.
void CHttpClient::handle_handshake(boost::system::error_code err)
{

	if(!err)
	{
		if (deadline_.expires_from_now(boost::posix_time::seconds(nTimeOut))>=0)
			deadline_.async_wait(boost::bind(&CHttpClient::check_deadline, this,boost::asio::placeholders::error));
		boost::asio::async_write(ssl_sock,boost::asio::buffer(m_body,m_bodyLen),
			boost::bind(&CHttpClient::handle_write,this,boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		mHttpBack(this,NULL,"SHAKEERR",0);
	}
}

void CHttpClient::handle_write(boost::system::error_code err,size_t bytes_transfarred)
{

	if(!err)
	{
		if (deadline_.expires_from_now(boost::posix_time::seconds(nTimeOut))>=0)
			deadline_.async_wait(boost::bind(&CHttpClient::check_deadline, this,boost::asio::placeholders::error));

		if(protocol_==1)
		{
			boost::asio::async_read_until(ssl_sock,respone_,"\r\n\r\n",
				boost::bind(&CHttpClient::handle_HeaderRead,this,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
		}
		else
		{
			boost::asio::async_read_until(socket_,respone_,"\r\n\r\n",
				boost::bind(&CHttpClient::handle_HeaderRead,this,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
		}
	}
	else
	{
		mHttpBack(this,NULL,"WRITEERR",0);
	}
}

//http包头的读取回调函数。
void CHttpClient::handle_HeaderRead(boost::system::error_code err,size_t bytes_transfarred)
{

	if(!err)
	{
		if (deadline_.expires_from_now(boost::posix_time::seconds(nTimeOut))>=0)
			deadline_.async_wait(boost::bind(&CHttpClient::check_deadline, this,boost::asio::placeholders::error));

		nHeaderLen=bytes_transfarred;
		std::istream response_stream(&respone_);
		response_stream.unsetf(std::ios_base::skipws);//asio::streambuf 转换成istream 并且忽略空格

		//将数据流追加到header里
		int readSize=respone_.size();

		char * head=new char[bytes_transfarred+1];
		ZeroMemory(head,bytes_transfarred+1);
		response_stream.read(head,bytes_transfarred);
		m_header=head;
		delete head;

		int rdContentSize=readSize-m_header.size();

		//获取httpContent的长度
		if(m_header.find("Content-Length")!=std::string::npos)
		{
			std::string len=m_header.substr(m_header.find("Content-Length: ")+16);
			len=len.substr(0,len.find_first_of("\r"));
			nContentLen=atoi(len.c_str());

			if(protocol_==1)
			{

				boost::asio::async_read(ssl_sock,respone_,boost::asio::transfer_at_least(nContentLen-rdContentSize)
					,boost::bind(&CHttpClient::handle_ContentRead,this,boost::asio::placeholders::error
					,boost::asio::placeholders::bytes_transferred));

			}
			else
			{
				boost::asio::async_read(socket_,respone_,boost::asio::transfer_at_least(nContentLen-rdContentSize)
					,boost::bind(&CHttpClient::handle_ContentRead,this,boost::asio::placeholders::error
					,boost::asio::placeholders::bytes_transferred));
			}

		}
		else if(m_header.find("Transfer-Encoding: chunked")!=std::string::npos)
		{
			if(protocol_==1)
			{

				boost::asio::async_read_until(ssl_sock,respone_,"\r\n"
					,boost::bind(&CHttpClient::handle_chunkRead,this,boost::asio::placeholders::error
					,boost::asio::placeholders::bytes_transferred));

			}
			else
			{
				boost::asio::async_read_until(socket_,respone_,"\r\n"
					,boost::bind(&CHttpClient::handle_chunkRead,this,boost::asio::placeholders::error
					,boost::asio::placeholders::bytes_transferred));
			}
		}


	}
	else
	{
		mHttpBack(this,NULL,"HEADREADERR",0);
	}


}

void CHttpClient::handle_chunkRead(boost::system::error_code err,size_t bytes_transfarred)
{
	if(!err||err.value()==2)
	{
		try{
		std::istream response_stream(&respone_);
		response_stream.unsetf(std::ios_base::skipws);//asio::streambuf 转换成istream 并且忽略空格

		int contSize=bytes_transfarred;
		int readLen=respone_.size()-contSize;

		char *chunkStr=new char[contSize]; //此处申请了内存，注意释放。
		response_stream.read(chunkStr,contSize);
		ZeroMemory(chunkStr+contSize-2,2);
		long nextReadSize=strtol(chunkStr,NULL,16);
		delete chunkStr;
		if(nextReadSize==0) 
		{
			std::string msg=m_respone;
			mHttpBack(this,this->m_respone,m_header,nContentLen);
			return ;
		}

		char * htmlBuf=new char[nextReadSize+2];
		ZeroMemory(htmlBuf,nextReadSize+2);
		if(nextReadSize>readLen){
			if(protocol_==1){
				boost::asio::read(ssl_sock,respone_,boost::asio::transfer_at_least(nextReadSize-readLen+2));
			}else{
				boost::asio::read(socket_,respone_,boost::asio::transfer_at_least(nextReadSize-readLen+2));
			}
		}

		response_stream.read(htmlBuf,nextReadSize+2);
		ZeroMemory(htmlBuf+nextReadSize,2);

		if(this->m_respone==NULL){
			this->m_respone=htmlBuf;
			nContentLen=nextReadSize;
		}else{
			char * newCont=new char[nContentLen+nextReadSize+1];
			ZeroMemory(newCont,nContentLen+nextReadSize+1);
			memcpy(newCont,this->m_respone,nContentLen);
			memcpy(newCont+nContentLen,htmlBuf,nextReadSize);
			delete  this->m_respone;
			delete  htmlBuf;

			this->m_respone=newCont;
			nContentLen+=nextReadSize;
		}

		if(protocol_==1){
			boost::asio::async_read_until(ssl_sock,respone_,"\r\n"
				,boost::bind(&CHttpClient::handle_chunkRead,this,boost::asio::placeholders::error
				,boost::asio::placeholders::bytes_transferred));
		}
		else
		{
			boost::asio::async_read_until(socket_,respone_,"\r\n"
				,boost::bind(&CHttpClient::handle_chunkRead,this,boost::asio::placeholders::error
				,boost::asio::placeholders::bytes_transferred));
		}
		}
		catch(const boost::system::error_code& ex)
		{
			this->m_header=ex.message();
			int a= ex.value();
			a=1;
		}

	}else{
		mHttpBack(this,NULL,"CONTENTREADERR",0);
	}

}

//http包体的读取回调函数
void CHttpClient::handle_ContentRead(boost::system::error_code err,size_t bytes_transfarred)
{
	if(!err||err.value()==2)
	{
		std::istream response_stream(&respone_);
		response_stream.unsetf(std::ios_base::skipws);//asio::streambuf 转换成istream 并且忽略空格

		nContentLen=respone_.size();
		//将数据流追加到header里
		char *cont=new char[nContentLen+1]; //此处申请了内存，注意释放。
		ZeroMemory(cont+nContentLen,1);
		response_stream.read(cont,nContentLen);

		mHttpBack(this,cont,m_header,nContentLen);

	}else
	{
		mHttpBack(this,NULL,"CONTENTREADERR",0);
	}
}

void CHttpClient::check_deadline(boost::system::error_code err)
{
	if(err != boost::asio::error::operation_aborted)
	{
		bStop=false;
		try{
			socket_.close();
			bStop=true;
		}
		catch(...)
		{
			bStop=true;
		}

	}

}
