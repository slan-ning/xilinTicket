#pragma once
#include "Http.h"
#include <map>

using namespace std;
class CBuyTicketDlg;
class CHuoche
{
private:
	CHttp *http;
	string uname;
	string upass;
	string yzcode;
	CBuyTicketDlg* dlg;
	std::string getSuggest(void);
	std::map<std::string,std::string> station;

	string fromCode;
	string toCode;
	string date;

public:
	CHuoche(CBuyTicketDlg *dlg);
	~CHuoche(void);

	void SetCookie(std::string cookies);
	
	bool Login(std::string username, std::string password, std::string code);
	bool GetCode(void);
	void SearchTicket(std::string fromStation,std::string toStation,std::string date);
	void RecvSchPiao(std::map<std::string,std::string> respone, char * msg , int nLen);
	
	void showMsg(std::string msg);
	bool submitOrder(std::string ticketinfo,std::string seat);
	void RecvSubmitOrder(std::map<std::string,std::string> respone, char * msg , int nLen,std::string seat);
	bool loadCode2(void);
	bool isInBuy;
	std::string train;
	void confrimOrder(std::map<std::string,std::string> respone, char * msg , int nLen,std::string pstr);
	bool m_Success;
};

