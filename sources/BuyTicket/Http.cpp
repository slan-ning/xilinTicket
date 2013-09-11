#include "StdAfx.h"
#include "Http.h"
#include "HttpClient.h"
#include "CIoPool.h"
#include "weblib.h"
#include <fstream>
#include <boost\algorithm\string.hpp>


CHttp::CHttp()
{
	this->m_ioServ=&CIoPool::Instance(4)->io;
	m_request["User-Agent"]="Mozilla/5.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64; Trident/5.0)";
	m_request["Accept-Language"]="zh-cn";
	m_request["Accept"]="*/*";
	m_request["Connection"]="Keep-Alive";
	m_request["Content-Type"]="text/html";
}

CHttp::~CHttp(void)
{

}

void CHttp::Send(std::string method,std::string url,std::string pstr,FuncCallBack funcBack)
{

	std::string host=weblib::substr(url,"://","/");
	int ssl=(url.find("https://")!=std::string::npos)?1:0;
	std::string port=this->GetPortByUrl(url);

	char * data=NULL;
	size_t dataLen=0;

	dataLen=pstr.length();
	data=new char[dataLen];
	memcpy(data,pstr.c_str(),dataLen);

	dataLen=this->BuildHttpBody(method,url, data , dataLen);

	CHttpClient *httpclient=new CHttpClient(boost::bind(&CHttp::MessageCallBack,this,_1,_2,_3,_4,funcBack),*m_ioServ);

	httpclient->Send(ssl,host,port,data,dataLen);

}

void CHttp::Send(std::string method, std::string url,char * data, size_t dataLen,FuncCallBack funcBack)
{
	std::string host=weblib::substr(url,"://","/");
	int ssl=(url.find("https://")!=std::string::npos)?1:0;
	std::string port=this->GetPortByUrl(url);

	dataLen=this->BuildHttpBody(method,url, data , dataLen);

	CHttpClient *httpclient=new CHttpClient(boost::bind(&CHttp::MessageCallBack,this,_1,_2,_3,_4,funcBack),*m_ioServ);

	httpclient->Send(ssl,host,port,data,dataLen);
}

//普通GET
void CHttp::AsyGet(std::string url,FuncCallBack funcBack)
{

	this->Send("GET",url,"",funcBack);
}

void CHttp::AsyDelete(std::string url,FuncCallBack funcBack)
{

	this->Send("DELETE",url,"",funcBack);
}

//普通post
void CHttp::AsyPost(std::string url,std::string postData,FuncCallBack funcBack)
{
	this->Send("POST",url,postData,funcBack);

}

//代理GET
void CHttp::AsyGet(std::string ip,std::string port,std::string url,FuncCallBack funcBack)
{
	std::string host=weblib::substr(url,"://","/");
	int ssl=0;//不是ssl连接

	char * data=NULL;
	size_t bodySize=this->BuildHttpBody("GET",url,data,0);

	CHttpClient *httpclient=new CHttpClient(boost::bind(&CHttp::MessageCallBack,this,_1,_2,_3,_4,funcBack),*m_ioServ);

	httpclient->Send(ssl,ip,port,data,bodySize);
}

//代理POST
void CHttp::AsyPost(std::string ip,std::string port,std::string url,std::string postData,FuncCallBack funcBack)
{
	std::string host=weblib::substr(url,"://","/");
	int ssl=0;//不是ssl连接

	char * data=NULL;
	size_t dataLen=0;
	dataLen=postData.length();
	data=new char[dataLen];
	memcpy(data,postData.c_str(),dataLen); //构造post表单数据

	dataLen=this->BuildHttpBody("POST",url, data , dataLen);

	CHttpClient *httpclient=new CHttpClient(boost::bind(&CHttp::MessageCallBack,this,_1,_2,_3,_4,funcBack),*m_ioServ);

	httpclient->Send(ssl,ip,port,data,dataLen);

}

std::string CHttp::Get(std::string url)
{
	std::string host=weblib::substr(url,"://","/");
	int ssl=(url.find("https://")!=std::string::npos)?1:0;
	std::string port=this->GetPortByUrl(url);

	char * data=NULL;
	size_t dataLen=0;


	dataLen=this->BuildHttpBody("GET",url, data , dataLen);

	CHttpClient *httpclient=new CHttpClient(*m_ioServ);

	int nCode=httpclient->SynSend(ssl,host,port,data,dataLen);

	this->BuildRespone(httpclient->m_header);
	this->BuildCookie(httpclient->m_header);

	if(nCode==0&&httpclient->m_respone!=NULL){
		std::string msg=(httpclient->m_respone!=NULL)?httpclient->m_respone:"";
		delete httpclient;
		return msg;
	}else{
		std::string msg=httpclient->m_header;
		delete httpclient;
		return msg;
	}



}

std::string CHttp::Post(std::string url,std::string pstr)
{
	std::string host=weblib::substr(url,"://","/");
	int ssl=(url.find("https://")!=std::string::npos)?1:0;
	std::string port=this->GetPortByUrl(url);

	char * data=NULL;
	size_t dataLen=0;

	dataLen=pstr.length();
	data=new char[dataLen];
	memcpy(data,pstr.c_str(),dataLen);

	dataLen=this->BuildHttpBody("POST",url, data , dataLen);

	CHttpClient *httpclient=new CHttpClient(*m_ioServ);

	int nCode=httpclient->SynSend(ssl,host,port,data,dataLen);

	this->BuildRespone(httpclient->m_header);
	this->BuildCookie(httpclient->m_header);

	if(nCode==0){
		std::string msg=(httpclient->m_respone!=NULL)?httpclient->m_respone:"";
		delete httpclient;
		return msg;
	}else{
		std::string msg=httpclient->m_header;
		delete httpclient;
		return msg;
	}

}

//代理GET
std::string CHttp::Get(std::string ip,std::string port,std::string url)
{
	std::string host=weblib::substr(url,"://","/");
	int ssl=0;//不是ssl连接

	char * data=NULL;
	size_t bodySize=this->BuildHttpBody("GET",url,data,0);

	CHttpClient *httpclient=new CHttpClient(*m_ioServ);

	int nCode=httpclient->SynSend(ssl,ip,port,data,bodySize);

	this->BuildRespone(httpclient->m_header);
	this->BuildCookie(httpclient->m_header);

	if(nCode==0){
		std::string msg=(httpclient->m_respone!=NULL)?httpclient->m_respone:"";
		delete httpclient;
		return msg;
	}else{
		std::string msg=httpclient->m_header;
		delete httpclient;
		return msg;
	}
}

//代理POST
std::string CHttp::Post(std::string ip,std::string port,std::string url,std::string postData)
{
	std::string host=weblib::substr(url,"://","/");
	int ssl=0;//不是ssl连接

	char * data=NULL;
	size_t dataLen=0;
	dataLen=postData.length();
	data=new char[dataLen];
	memcpy(data,postData.c_str(),dataLen); //构造post表单数据

	dataLen=this->BuildHttpBody("POST",url, data , dataLen);

	CHttpClient *httpclient=new CHttpClient(*m_ioServ);

	int nCode=httpclient->SynSend(ssl,ip,port,data,dataLen);

	this->BuildRespone(httpclient->m_header);
	this->BuildCookie(httpclient->m_header);

	if(nCode==0){
		std::string msg=(httpclient->m_respone!=NULL)?httpclient->m_respone:"";
		delete httpclient;
		return msg;
	}else{
		std::string msg=httpclient->m_header;
		delete httpclient;
		return msg;
	}

}


void CHttp::MessageCallBack(CHttpClient* sender, char* retnMsg, std::string header, int ContentLen,FuncCallBack funback)
{
	if(sender!=NULL)
	{
		while (sender->bStop==false)
		{
			Sleep(2);
		}	
	}


	this->BuildCookie(header);
	this->BuildRespone(header);

	if(header.find("charset=UTF-8")!=std::string::npos||header.find("charset=utf-8")!=std::string::npos)
	{
		char *newbuf=NULL;
		ContentLen=Utf8ToAnsi(retnMsg,&newbuf);
		delete retnMsg;
		retnMsg=newbuf;
	}

	if(funback!=NULL)
	{
		funback(this->m_respone,retnMsg,ContentLen);
	}
	delete retnMsg;
	delete sender;
	sender=NULL;
	
}


size_t CHttp::BuildHttpBody(std::string method, std::string url, char * &data ,size_t dataLen)
{
	std::string turl=url;//保存url

	std::string host=weblib::substr(url,"://","/");
	url=url.substr(url.find(host)+host.length());
	if(url=="") url="/";
	std::string body;

	if(method=="POST")
	{
		char len[20];
		::sprintf_s(len,"%d",dataLen);
		m_request["Content-Type"]="application/x-www-form-urlencoded";
		m_request["Content-Length"]=std::string(len);
	}else{
		m_request["Content-Type"]="";
		m_request["Content-Length"]="";
	}

	body=method+" "+url+" HTTP/1.1\r\n";

	std::map<std::string,std::string>::iterator itr=m_request.begin();

	for(;itr!=m_request.end();itr++)
	{
		if(itr->second!="")
			body+=itr->first+": "+itr->second+"\r\n";
	}
	body+="Host: "+host+"\r\n";

	body+="\r\n";
	size_t len=0;

	if(dataLen>0)
	{
		len=body.length()+dataLen;
		char * newdata=new char[len];
		ZeroMemory(newdata,len);
		memcpy(newdata,body.c_str(),body.length());
		memcpy(newdata+body.length(),data,dataLen);
		delete data;
		data=newdata;
	}else
	{
		len=body.length();
		data=new char[len];
		ZeroMemory(data,len);
		memcpy(data,body.c_str(),len);
	}

	m_respone.clear();

	return len;

}


std::string CHttp::GetPortByUrl(std::string url)
{
	bool ssl=false;
	if(url.find("http://")!=std::string::npos)
	{
		url=url.substr(7);
		ssl=false;
	}
	if(url.find("https://")!=std::string::npos)
	{
		url=url.substr(8);
		ssl=true;
	}

	std::string port=url.substr(0,url.find_first_of('/'));
	if(port.find(":")!=std::string::npos)
	{
		port=port.substr(port.find_first_of(':'));
	}
	else if(ssl)
	{
		port="443";
	}
	else
	{
		port="80";
	}

	return port;

}


void CHttp::BuildCookie(std::string header)
{
	boost::smatch result;
	std::string regtxt("Set-Cooki.*? (.*?)=(.*?);");
	boost::regex rx(regtxt);

	std::string::const_iterator it=header.begin();
	std::string::const_iterator end=header.end();

	while (regex_search(it,end,result,rx))
	{
		std::string cookie_key=result[1];
		std::string cookie_value=result[2];

		if (m_request["Cookie"].find(cookie_key)==std::string::npos)
		{
			m_request["Cookie"]+=cookie_key+"="+cookie_value+"; ";
		}
		else
		{
			std::string reg="("+cookie_key+")=(.*?);";
			boost::regex regrep(reg,    boost::regex::icase|boost::regex::perl);
			m_request["Cookie"]=boost::regex_replace(m_request["Cookie"],regrep,"$1="+cookie_value+";");
		}

		it=result[0].second;
	}

}

void CHttp::FakeIp(void)
{
	srand( (unsigned)time( NULL ) );
	std::string ip;
	char cip[5];

	sprintf_s(cip,"%d",rand()%253+1);
	ip=cip;
	ip+=".";

	sprintf_s(cip,"%d",rand()%253+1);
	ip+=cip;
	ip+=".";

	sprintf_s(cip,"%d",rand()%253+1);
	ip+=cip;
	ip+=".";

	sprintf_s(cip,"%d",rand()%253+1);
	ip+=cip;

	m_request["X-Forwarded-For"]=ip;

}

int CHttp::Utf8ToAnsi(const char* buf,char **newbuf)
{
	int nLen = ::MultiByteToWideChar(CP_UTF8,0,buf,-1,NULL,0);  //返回需要的unicode长度

	WCHAR * wszANSI = new WCHAR[nLen+1];
	memset(wszANSI, 0, nLen * 2 + 2);

	nLen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, wszANSI, nLen);    //把utf8转成unicode

	nLen = WideCharToMultiByte(CP_ACP, 0, wszANSI, -1, NULL, 0, NULL, NULL);        //得到要的ansi长度

	*newbuf=new char[nLen + 1];
	memset(*newbuf, 0, nLen + 1);
	WideCharToMultiByte (CP_ACP, 0, wszANSI, -1, *newbuf, nLen, NULL,NULL);          //把unicode转成ansi

	return nLen;

}


void CHttp::BuildRespone(std::string header)
{
	if(header.find("HTTP")!=std::string::npos)
	{
		std::string h=header.substr(header.find(" ")+1);
		h=h.substr(0,h.find(" "));
		this->m_respone["responeCode"]=h;

		boost::smatch result;
		std::string regtxt("\\b(\\w+?): (.*?)\r\n");
		boost::regex rx(regtxt);

		std::string::const_iterator it=header.begin();
		std::string::const_iterator end=header.end();

		while (regex_search(it,end,result,rx))
		{
			std::string key=result[1];
			std::string value=result[2];
			m_respone[key]=value;
			it=result[0].second;
		}
	}else
	{
		this->m_respone["responeCode"]="-1";
	}
}



bool CHttp::GetFile(std::string url, std::string path)
{
	std::string host=weblib::substr(url,"://","/");
	int ssl=(url.find("https://")!=std::string::npos)?1:0;
	std::string port=this->GetPortByUrl(url);

	char * data=NULL;
	size_t dataLen=0;


	dataLen=this->BuildHttpBody("GET",url, data , dataLen);

	CHttpClient *httpclient=new CHttpClient(*m_ioServ);

	int nCode=httpclient->SynSend(ssl,host,port,data,dataLen);

	this->BuildRespone(httpclient->m_header);
	this->BuildCookie(httpclient->m_header);

	if(nCode==0&&httpclient->m_respone!=NULL){
		std::ofstream fs(path,ios::binary);
		if(fs.is_open()){
			fs.write(httpclient->m_respone,httpclient->m_resLen);
			fs.close();
			delete httpclient;
			return true;
		}else{
			return false;
		}

	}else{
		delete httpclient;
		return false;
	}



	return false;
}
