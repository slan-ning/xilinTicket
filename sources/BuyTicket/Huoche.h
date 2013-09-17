#pragma once
#include <map>
#include <boost/smart_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

using namespace std;
class CBuyTicketDlg;
namespace echttp{
	class http;
	class respone;
}
class CHuoche
{
private:
	echttp::http *http;
	string uname;
	string upass;
	string yzcode;
	CBuyTicketDlg* dlg;
	std::string getSuggest(void);
	std::map<std::string,std::string> station;

	string fromCode;
	string toCode;
	string date;
    string encrypt_str;

	bool isTicketEnough(std::string tickstr);
    std::string xxtea_encode(std::string data,std::string key);
    void encrypt_code(std::string src);

public:
	CHuoche(CBuyTicketDlg *dlg);
	~CHuoche(void);

	void SetCookie(std::string cookies);
	
	bool Login(std::string username, std::string password, std::string code);
	bool GetCode(void);
	void SearchTicket(std::string fromStation,std::string toStation,std::string date);
	void RecvSchPiao(boost::shared_ptr<echttp::respone> respone);
	
	void showMsg(std::string msg);
	bool submitOrder(std::string ticketinfo,std::string seat);
	void RecvSubmitOrder(boost::shared_ptr<echttp::respone> respone, std::string seat);
	bool loadCode2(void);
	bool isInBuy;
	std::string train;
	void confrimOrder(boost::shared_ptr<echttp::respone> respone, std::string pstr);
	bool m_Success;
};

