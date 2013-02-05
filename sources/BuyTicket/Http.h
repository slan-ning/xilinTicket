#pragma once
#include <boost\regex.hpp>
#include <boost\bind.hpp>
#include <boost\asio.hpp>
#include <boost\function.hpp>
#include <map>

class CHttpClient;

class CHttp
{
public:
	typedef boost::function<void(std::map<std::string,std::string>,char*,int)> FuncCallBack;
	CHttp();
	~CHttp(void);
	virtual void AsyGet(std::string url,FuncCallBack funcBack);
	virtual void AsyDelete(std::string url,FuncCallBack funcBack);
	virtual void AsyPost(std::string url,std::string postData,FuncCallBack funcBack);
	virtual void AsyGet(std::string ip,std::string port,std::string url,FuncCallBack funcBack);
	virtual void AsyPost(std::string ip,std::string port,std::string url,std::string postData,FuncCallBack funcBack);
	virtual void Send(std::string method,std::string url,std::string pstr,FuncCallBack funcBack);
	void Send(std::string method, std::string url,char * data, size_t dataLen,FuncCallBack funcBack);
	void MessageCallBack(CHttpClient* sender,char* retnMsg, std::string header, int ContentLen,FuncCallBack funback);

	std::string Get(std::string url);
	std::string Post(std::string url,std::string data);
	std::string Get(std::string ip,std::string port,std::string url);
	std::string Post(std::string ip,std::string port,std::string url,std::string postData);
	bool GetFile(std::string url, std::string path);

	void BuildCookie(std::string header);
	void FakeIp(void);
	std::map<std::string,std::string> m_request;
	std::map<std::string,std::string> m_respone;

private:

	size_t BuildHttpBody(std::string method, std::string url, char * &data ,size_t dataLen);
	std::string GetPortByUrl(std::string url);
	boost::asio::io_service *m_ioServ;
	int Utf8ToAnsi(const char* buf,char** newbuf);
	void BuildRespone(std::string header);

public:
	
};

