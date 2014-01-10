#include "stdafx.h"
#include "echttp/http.hpp"
#include "Huoche.h"
#include <iostream>
#include <fstream>
#include "BuyTicketDlg.h"
#include "YzDlg.h"
#include <WinInet.h>
#include "xxtea.h"
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include "Ticket.hpp"

#pragma comment(lib,"Wininet.lib")

boost::random::mt19937 rand_gen;

std::string CHuoche::xxtea_encode(std::string data_,std::string key_)
{
    xxtea_long ret_len=0;
    boost::shared_array<char> v(new char[data_.size()*2]);//*2 because need enough to storage buf in encode inside.
    memset(v.get(),0,data_.size()*2);
    memcpy(v.get(),data_.c_str(),data_.size());

    boost::shared_array<char> k(new char[key_.size()*2]);
    memset(k.get(),0,key_.size()*2);
    memcpy(k.get(),key_.c_str(),key_.size());

    unsigned char * ret=xxtea_encrypt((unsigned char *)v.get(),data_.size(),(unsigned char *)k.get(),&ret_len);

    char* buf=new char[ret_len*2+1];
    memset(buf,0,ret_len+1);

    for( int i = 0; i < ret_len; ++i)
    {
        unsigned char c=*(ret+i);
        sprintf(buf+2*i, "%02x", c);
    }

    std::string result=echttp::base64_encode((unsigned char*)buf,ret_len*2);
    delete[] buf;
    free(ret);

    return result;
}


CHuoche::CHuoche(CBuyTicketDlg *dlg)
	: isInBuy(false)
	, m_Success(false)
{
	this->dlg=dlg;
	http=new echttp::http();
    this->buy_list=new std::queue<Ticket>();
    this->LoadStation();

	http->Request.set_defalut_userAgent("Mozilla/5.0 (compatible; MSIE 9.0; qdesk 2.5.1177.202; Windows NT 6.1; WOW64; Trident/6.0)");
	this->LoadDomain();

	this->http->Get("https://"+this->domain+"/otn/");
	this->http->Request.m_header.insert("Referer","https://kyfw.12306.cn/otn/");


	std::string initUrl=this->http->Get("https://"+this->domain+"/otn/login/init")->as_string();

    if(initUrl.find("/otn/dynamicJs/loginJs")!=std::string::npos)
    {
        std::string loginjs=echttp::substr(initUrl,"/otn/dynamicJs/loginJs","\"");
	    std::string loginjs2=echttp::substr(initUrl,"/otn/resources/merged/login_js.js?scriptVersion=","\"");

	    this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/login/init");
	    this->http->Get("https://"+this->domain+"/otn/resources/merged/login_js.js?scriptVersion="+loginjs2);
	    std::string ret= this->http->Get("https://"+this->domain+"/otn/dynamicJs/loginJs"+loginjs)->as_string();
        this->encrypt_code(ret);

		//判断是否有隐藏随机监测url
		std::string ready_str=echttp::substr(ret,"$(document).ready(function(){","success");
		if(ready_str.find("jq({url :'")!=std::string::npos)
		{
			std::string url=echttp::substr(ready_str,"jq({url :'","'");
			this->http->Post("https://"+this->domain+""+url,"");
		}

    }else
    {
        this->dlg->m_listbox.AddString("获取登录信息异常！");
    }
}


CHuoche::~CHuoche(void)
{
	delete http;
}

//从加密js生成验证信息
void CHuoche::encrypt_code(std::string src)
{
    if(src.find("gc(){var key='")==std::string::npos){
        this->encrypt_str="";
    }else{  
        std::string key=echttp::substr(src,"gc(){var key='","';");
        std::string code=this->xxtea_encode("1111",key);

        this->encrypt_str="&"+echttp::UrlEncode(key)+"="+echttp::UrlEncode(code);
    }

}

bool CHuoche::Login(std::string username, std::string password, std::string code)
{
	this->uname=username;
	this->upass=password;
	this->yzcode=code;
	std::ofstream file("c:\\登录错误.txt",std::ios::app);
	this->http->Request.m_header.insert("x-requested-with","XMLHttpRequest");
	//std::string num=this->getSuggest();
    std::string pstr="loginUserDTO.user_name="+this->uname+"&userDTO.password="+this->upass+"&randCode="+this->yzcode;
	
	
	std::string res=this->http->Post("https://"+this->domain+"/otn/login/loginAysnSuggest",pstr)->as_string();
	res=echttp::Utf8Decode(res);

	if(res.find("{\"loginCheck\":\"Y\"}")!=std::string::npos){
		this->dlg->m_listbox.AddString("登录成功");
		this->SetCookie(this->http->Request.m_cookies.cookie_string());
        this->http->Post("https://"+this->domain+"/otn/login/init","_json_att=");
		return true;
    }else if(res.find("验证码不正确")!=std::string::npos){
        this->dlg->m_listbox.AddString("验证码不正确");
		return false;
    }else{
		file<<res;
		file.close();
		this->showMsg("登录失败："+echttp::substr(res,"messages\":[\"","]"));
		return false;
	}
	
}


bool CHuoche::GetCode(void)
{
	this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/login/init");
	int status_code=this->http->Get(std::string("https://"+this->domain+"/otn/passcodeNew/getPassCodeNew?module=login&rand=sjrand"),std::string("c:\\buyticket.png"))->status_code;
	return status_code==200;
}


std::string CHuoche::getSuggest(void)
{
	std::string res=this->http->Post("https://dynamic.12306.cn/otsweb/loginAction.do?method=loginAysnSuggest","")->as_string();
	if(res=="") return "";
	return echttp::substr(res,"{\"loginRand\":\"","\"");
}

//访问查询余票信息页面（定时访问，以保持在线）
void CHuoche::SerachTicketPage()
{
	this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/leftTicket/init");
    this->http->Get("https://"+this->domain+"/otn/leftTicket/init",boost::bind(&CHuoche::RecvSearchTicketPage,this,_1));
    this->CheckUserOnline();
}

void CHuoche::RecvSearchTicketPage(boost::shared_ptr<echttp::respone> respone)
{
    std::string sources;
	if (respone->as_string()!=""){
		sources=respone->as_string();

        if(sources.find("/otn/dynamicJs/queryJs")!=std::string::npos)
        {
            std::string authJs=echttp::substr(sources,"/otn/dynamicJs/queryJs","\"");

	        this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/leftTicket/init");
	        std::string ret= this->http->Get("https://"+this->domain+"/otn/dynamicJs/queryJs"+authJs)->as_string();
            this->encrypt_code(ret);

		    //判断是否有隐藏随机监测url
		    std::string ready_str=echttp::substr(ret,"$(document).ready(function(){","success");
		    if(ready_str.find("jq({url :'")!=std::string::npos)
		    {
			    std::string url=echttp::substr(ready_str,"jq({url :'","'");
			    this->http->Post("https://"+this->domain+""+url,"");
		    }
		
        }else
        {
            this->showMsg("获取查询车票页面异常！");
        }

    }else{
        this->showMsg("查票页面已空白!!!");
    }
}

void CHuoche::SearchTicket(std::string fromStation,std::string toStation,std::string date)
{
    this->LoadDomain();

	//http->Request.set_defalut_userAgent("Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; WOW64; Trident/6.0)");

	this->fromCode=station[fromStation];
	this->toCode=station[toStation];
	this->date=date;
	this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/leftTicket/init");

    this->http->Request.m_header.insert("x-requested-with","XMLHttpRequest");
    this->http->Request.m_header.insert("Content-Type","application/x-www-form-urlencoded");
	std::string url="https://"+this->domain+"/otn/leftTicket/query?leftTicketDTO.train_date="+this->date
                    +"&leftTicketDTO.from_station="+this->fromCode+"&leftTicketDTO.to_station="+this->toCode
                    +"&purpose_codes=ADULT";
	this->http->Get(url,boost::bind(&CHuoche::RecvSchPiao,this,_1));

	//5 percent to flush search page; 
	boost::random::uniform_int_distribution<> dist(1, 60);
	if(dist(rand_gen)==20)
	{
		this->SerachTicketPage();
	}

}

bool CHuoche::isTicketEnough(std::string tickstr)
{
	if(tickstr=="--" || tickstr=="*" || tickstr.find("无")!=std::string::npos)
		return false;

	if(tickstr.find("有")!=std::string::npos)
		return true;

	CString fullname2;
	this->dlg->GetDlgItem(IDC_FULLNAME2)->GetWindowText(fullname2);
	fullname2.Trim();

	CString fullname3;
	this->dlg->GetDlgItem(IDC_FULLNAME2)->GetWindowText(fullname3);
	fullname3.Trim();

	int ticket_num=atoi(tickstr.c_str());

	if(!fullname3.IsEmpty())
	{
		return ticket_num>=3;
	}
	else if(!fullname2.IsEmpty())
	{
		return ticket_num>=2;
	}else
	{
		return ticket_num>=1;
	}

}

void CHuoche::RecvSchPiao(boost::shared_ptr<echttp::respone> respone)
{
	std::string restr;
	
	restr=echttp::Utf8Decode(respone->as_string());

	if(restr!=""&&restr!="-10" &&restr.find("queryLeftNewDTO")!=std::string::npos){
		std::ofstream webfile("c:\\web.txt",std::ios::app);
		webfile<<restr<<"\r\n";
		webfile.close();

		while(restr.find("queryLeftNewDTO")!=std::string::npos)
		{
            std::string ticketInfo=echttp::substr(restr,"queryLeftNewDTO","{\"queryLeftNewDTO");

            Ticket ticket(ticketInfo);

			restr=restr.substr(restr.find("queryLeftNewDTO")+5);

			std::string trainstr=echttp::substr(restr,"_train_code\":\"","\"");
			if(this->train!=""&&this->train.find(trainstr)==std::string::npos)
			{
				continue;
			}

            if(this->isTicketEnough(ticket.first_seat)&&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_YDZ))
			{
				if(restr.find("secretStr")!=std::string::npos)
				{
					this->showMsg(trainstr+"有一等座"+"---车票数目:"+ticket.first_seat);
                    ticket.SetBuySeat("M");
                    buy_list->push(ticket);
				}else{
					this->showMsg("未知一等座余票信息");
				}
				

			}
            
			if(this->isTicketEnough(ticket.second_seat)&&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_EDZ))
			{
				if(restr.find("secretStr")!=std::string::npos)
				{
                    ticket.SetBuySeat("O");
                    buy_list->push(ticket);
					this->showMsg(trainstr+"有二等座"+"---车票数目:"+ticket.second_seat);
				}else{
					this->showMsg("未知二等座余票信息");
				}
				

			}
			
			if(this->isTicketEnough(ticket.soft_bed)&&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_RW))
			{
				if(restr.find("secretStr")!=std::string::npos)
				{
					ticket.SetBuySeat("4");
                    buy_list->push(ticket);
					this->showMsg(trainstr+"有软卧"+"---车票数目:"+ticket.soft_bed);
				}else{
					this->showMsg("未知软卧余票信息");
				}
				

			}
            
			if(this->isTicketEnough(ticket.hard_bed)&&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_YW))
			{
				if(restr.find("secretStr")!=std::string::npos)
				{
					ticket.SetBuySeat("3");
                    buy_list->push(ticket);
                    this->showMsg(trainstr+"有卧铺"+"---车票数目:"+ticket.hard_bed);
				}else{
					this->showMsg("未知卧铺余票信息");
				}
				

            }
			
			if(this->isTicketEnough(ticket.hard_seat) &&BST_CHECKED==this->dlg->IsDlgButtonChecked(IDC_CHECK_YZ))
			{
                this->showMsg(trainstr+"有硬座"+"---车票数目:"+ticket.hard_seat);
				
				if(restr.find("secretStr")!=std::string::npos)
				{
					ticket.SetBuySeat("1");
                    buy_list->push(ticket);
				}else{
					this->showMsg("未知硬座信息");
				}
				
			}

		}

		//如果检测到相应的票，就下单
        int queue_size=buy_list->size();
		if(queue_size>0 && !buy_list->empty()){

            Ticket task_ticket=buy_list->front();
            this->submitOrder(task_ticket);
            this->showMsg("开始购买:"+task_ticket.station_train_code);

            while (buy_list->size()>0)
            {
                if(queue_size==buy_list->size()) //如果当前队列数，和之前保留的队列数一致，则有任务在执行
                {    
                    continue;
                    Sleep(50);
                }

                queue_size=buy_list->size();
				if(!buy_list->empty())
				{
					Ticket task_ticket=buy_list->front();
					this->submitOrder(task_ticket);
					this->showMsg("开始购买:"+task_ticket.station_train_code);
				}
            }
		}else{
			this->showMsg("没有卧铺或者硬座");
		}

	}else{
		this->showMsg("发生错误，请检查参数:"+echttp::Utf8Decode(echttp::substr(restr,"\"messages\":\[\"","\"")));
	}
	

	

}


void CHuoche::showMsg(std::string msg)
{

	CTime tm;
	tm=CTime::GetCurrentTime();
	string str=tm.Format("%m月%d日 %X：");
	str+=msg;
	dlg->m_listbox.InsertString(0,str.c_str());

	std::ofstream file("c:\\ticketlog.txt",std::ios::app);
	file<<str<<"\r\n";
	file.close();

}


bool CHuoche::submitOrder(Ticket ticket)
{
	this->isInBuy=true;

    std::string pstr="secretStr="+ticket.secret_str+"&train_date="+ticket.train_date+"&back_train_date=2014-01-01&tour_flag=dc&purpose_codes=ADULT"+
        "&query_from_station_name="+ticket.from_station_name+"&query_to_station_name="+ticket.to_station_name+"&undefined";

	this->showMsg("尝试买:"+ticket.station_train_code);

	boost::shared_ptr<echttp::respone> ret=this->http->Post("https://"+this->domain+"/otn/leftTicket/submitOrderRequest",pstr);
	std::string recvStr=ret->as_string();
    if(recvStr.find("status\":true")!=std::string::npos)
	{
		this->http->Post("https://"+this->domain+"/otn/confirmPassenger/initDc","_json_att=",boost::bind(&CHuoche::RecvSubmitOrder,this,_1,ticket));
	}else{
		this->showMsg("预定错误!"+echttp::Utf8Decode(echttp::substr(recvStr,"\"messages\":\[\"","\"")));
        this->buy_list->pop();
	}
	
	return false;
}

void CHuoche::RecvSubmitOrder(boost::shared_ptr<echttp::respone> respone,Ticket ticket)
{
	std::string restr;
	restr=echttp::Utf8Decode(respone->as_string());

    this->LoadPassanger();

	this->http->Request.set_defalut_referer("https://kyfw.12306.cn/otn/confirmPassenger/initDc");

	
	//if(restr.find("/otsweb/dynamicJsAction.do")!=std::string::npos)
 //   {
 //       std::string authJs=echttp::substr(restr,"/otsweb/dynamicJsAction.do","\"");
	//    std::string ret= this->http->Get("https://dynamic.12306.cn/otsweb/dynamicJsAction.do"+authJs)->as_string();

	//	//判断是否有隐藏随机监测url
	//	std::string ready_str=echttp::substr(ret,"$(document).ready(function(){","success");
	//	if(ready_str.find("jq({url :'")!=std::string::npos)
	//	{
	//		std::string url=echttp::substr(ready_str,"jq({url :'","'");
	//		this->http->Post("https://dynamic.12306.cn"+url,"");
	//	}
 //   }

	string TOKEN=echttp::substr(restr,"globalRepeatSubmitToken = '","'");
    string keyCheck=echttp::substr(restr,"'key_check_isChange':'","'");
	string leftTicketStr=echttp::substr(restr,"leftTicketStr':'","'");
    string trainLocation=echttp::substr(restr,"train_location':'","'");
	
	
	string checkbox2="";

	string seattype=ticket.seat_type;//座位类型 3为卧铺 1为硬座
	
	
	std::string code_path=this->loadCode2();
	CYzDlg yzDlg;
	yzDlg.huoche=this;//传递本类指针到对话框
    yzDlg.file_path=code_path;
	if(yzDlg.DoModal()){
		this->showMsg("延时一下，过快会被封！");
		//Sleep(1000);
checkcode:
		std::string randcode=yzDlg.yzcode;
		/*std::string user2info="oldPassengers=&checkbox9=Y";
		this->http->Get("https://dynamic.12306.cn/otsweb/order/traceAction.do?method=logClickPassenger&passenger_name="+myName+"&passenger_id_no="+IdCard+"&action=checked");
		if(myName2!="")
		{
			user2info="passengerTickets="+seattype+"%2C0%2C1%2C"+myName2+"%2C1%2C"+IdCard2+"%2C"+Phone2
				+"%2CY&oldPassengers="+myName2+"%2C1%2C"+IdCard2+"&passenger_2_seat="
				+seattype+"&passenger_2_ticket=1&passenger_2_name="
				+myName2+"&passenger_2_cardtype=1&passenger_2_cardno="
				+IdCard2+"&passenger_2_mobileno="+Phone2
				+"&checkbox9=Y";
			checkbox2="&checkbox1=1";

			this->http->Get("https://dynamic.12306.cn/otsweb/order/traceAction.do?method=logClickPassenger&passenger_name="+myName2+"&passenger_id_no="+IdCard2+"&action=checked");

		}*/
		std::string passanger_info;

		if(passanger_name2!="")
		{
			passanger_info=seattype+"%2C0%2C1%2C"+this->passanger_name+"%2C1%2C"+this->passanger_idcard+"%2C"+this->passanger_phone
            +"%2CN_"+seattype+"%2C0%2C1%2C"+this->passanger_name1+"%2C1%2C"+this->passanger_idcard1+"%2C"+this->passanger_phone1
            +"%2CN_"+seattype+"%2C0%2C1%2C"+this->passanger_name2+"%2C1%2C"+this->passanger_idcard2+"%2C"+this->passanger_phone2
            +"%2CN&oldPassengerStr="+passanger_name+"%2C1%2C"+passanger_idcard+"%2C1_"+passanger_name1+"%2C1%2C"+passanger_idcard1+"%2C1_"+passanger_name2+"%2C1%2C"+passanger_idcard2+"%2C1_";
		}
		else if(passanger_name1=="")
		{
			passanger_info=seattype+"%2C0%2C1%2C"+this->passanger_name+"%2C1%2C"+this->passanger_idcard+"%2C"+this->passanger_phone
            +"%2CN_"+seattype+"%2C0%2C1%2C"+this->passanger_name1+"%2C1%2C"+this->passanger_idcard1+"%2C"+this->passanger_phone1
            +"%2CN&oldPassengerStr="+passanger_name+"%2C1%2C"+passanger_idcard+"%2C1_"+passanger_name1+"%2C1%2C"+passanger_idcard1+"%2C1_";
					
		}else{
			passanger_info=seattype+"%2C0%2C1%2C"+this->passanger_name+"%2C1%2C"+this->passanger_idcard+"%2C"+this->passanger_phone
            +"%2CN&oldPassengerStr="+passanger_name+"%2C1%2C"+passanger_idcard+"%2C1_";
		}

        std::string pstr="cancel_flag=2&bed_level_order_num=000000000000000000000000000000&passengerTicketStr="+passanger_info+"&tour_flag=dc&randCode="+randcode
            +"&_json_att=&REPEAT_SUBMIT_TOKEN="+TOKEN;

		string url="https://"+this->domain+"/otn/confirmPassenger/checkOrderInfo";

		string checkstr=this->http->Post(url,pstr)->as_string();
		checkstr=echttp::Utf8Decode(checkstr);

		if(checkstr.find("submitStatus\":true")!=std::string::npos 
			&& checkstr.find("status\":true")!=std::string::npos)
		{
            //检查排队队列，防止白提交
            bool ticket_enough=this->CheckQueueCount(ticket,TOKEN);
			
            if(!ticket_enough){
                this->showMsg("很遗憾,"+ticket.station_train_code+"排队人数已满");
                buy_list->pop();
                return;
            }

			this->http->Request.m_header.insert("Content-Type","application/x-www-form-urlencoded; charset=UTF-8");
			this->http->Request.m_header.insert("X-Requested-With","XMLHttpRequest");

            pstr="passengerTicketStr="+passanger_info+"&randCode="+randcode+"&purpose_codes=00"+
            +"&key_check_isChange="+keyCheck+"&leftTicketStr="+leftTicketStr+"&train_location="+trainLocation
            +"&_json_att=&REPEAT_SUBMIT_TOKEN="+TOKEN;
            //提交订单
			this->http->Post("https://"+this->domain+"/otn/confirmPassenger/confirmSingleForQueue",pstr,boost::bind(&CHuoche::confrimOrder,this,_1,pstr));
		}else if(checkstr.find("请重试")!=std::string::npos){
			this->showMsg(checkstr);
			Sleep(1000);
			goto checkcode;
		}
		else if(checkstr.find("验证码")!=std::string::npos)
		{
			this->showMsg("验证码错误，请重新输入！");
			int ret=yzDlg.DoModal();
			if(ret==IDOK){
				goto checkcode;
			}else if(ret==IDCANCEL) {
				this->showMsg("您选择了取消购票！");
                this->buy_list->pop();
			}
		}
		else{
            this->showMsg("检查订单操作错误:"+echttp::substr(checkstr,"errMsg","}"));
            this->buy_list->pop();
		}


	}

}

void CHuoche::SetCookie(std::string cookies)
{
	while(cookies.find(";")!=std::string::npos)
	{
		std::string cookie=cookies.substr(0,cookies.find_first_of(" "));
		cookie=cookie+"expires=Sun,22-Feb-2099 00:00:00 GMT";
		::InternetSetCookieA("https://kyfw.12306.cn",NULL,cookie.c_str());
		cookies=cookies.substr(cookies.find_first_of(" ")+1);
	}
}

std::string CHuoche::loadCode2(void)
{
    boost::random::uniform_int_distribution<> dist1(1, 50000);
    rand_gen.seed(time(0));
    int randcode=dist1(rand_gen);
	std::string randstr=echttp::convert<std::string>(randcode);
    std::string yz_code="c:\\12306\\buyticket"+randstr+".png";

	this->http->Request.m_header.insert("Referer","https://kyfw.12306.cn/otn/confirmPassenger/initDc");
	this->http->Get(std::string("https://"+this->domain+"/otn/passcodeNew/getPassCodeNew?module=passenger&rand=randp"),yz_code);
    return yz_code;
}


void CHuoche::confrimOrder(boost::shared_ptr<echttp::respone> respone,std::string pstr)
{
	std::string result;
	if (respone->as_string()!=""){
		result=echttp::Utf8Decode(respone->as_string());
		if(result.find("status\":true")!=std::string::npos)
		{
				this->m_Success=true;
				this->showMsg("!!!!!预定成功，速速到12306付款");
				this->buy_list->pop();
		}else if(result.find("请重试")!=std::string::npos)
		{
			if(!this->m_Success){
				Sleep(1000);
				this->http->Post("https://"+this->domain+"/otn/confirmPassenger/confirmSingleForQueue",pstr,boost::bind(&CHuoche::confrimOrder,this,_1,pstr));
				this->showMsg(result+"继续抢!");
			}
		}
		else
		{
				this->showMsg(result);
				this->buy_list->pop();
		}
	}else{
		if(!this->m_Success){
			this->http->Post("https://"+this->domain+"/otn/confirmPassenger/confirmSingleForQueue",pstr,boost::bind(&CHuoche::confrimOrder,this,_1,pstr));
			this->showMsg("返回空，继续抢!");
		}
	}
	

	return ;
}


void CHuoche::LoadStation(void)
{

    station["北京北"]="VAP";
	station["北京东"]="BOP";
	station["北京"]="BJP";
	station["北京南"]="VNP";
	station["北京西"]="BXP";
	station["重庆北"]="CUW";
	station["重庆"]="CQW";
	station["重庆南"]="CRW";
	station["上海"]="SHH";
	station["上海南"]="SNH";
	station["上海虹桥"]="AOH";
	station["上海西"]="SXH";
	station["天津北"]="TBP";
	station["天津"]="TJP";
	station["天津南"]="TIP";
	station["天津西"]="TXP";
	station["长春"]="CCT";
	station["长春南"]="CET";
	station["长春西"]="CRT";
	station["成都东"]="ICW";
	station["成都南"]="CNW";
	station["成都"]="CDW";
	station["长沙"]="CSQ";
	station["长沙南"]="CWQ";
	station["福州"]="FZS";
	station["福州南"]="FYS";
	station["贵阳"]="GIW";
	station["广州北"]="GBQ";
	station["广州东"]="GGQ";
	station["广州"]="GZQ";
	station["广州南"]="IZQ";
	station["哈尔滨"]="HBB";
	station["哈尔滨东"]="VBB";
	station["哈尔滨西"]="VAB";
	station["合肥"]="HFH";
	station["合肥西"]="HTH";
	station["呼和浩特东"]="NDC";
	station["呼和浩特"]="HHC";
	station["海口东"]="HMQ";
	station["海口"]="VUQ";
	station["杭州"]="HZH";
	station["杭州南"]="XHH";
    station["杭州东"]="HGH";
	station["济南"]="JNK";
	station["济南东"]="JAK";
	station["济南西"]="JGK";
	station["昆明"]="KMM";
	station["昆明西"]="KXM";
	station["拉萨"]="LSO";
	station["兰州东"]="LVJ";
	station["兰州"]="LZJ";
	station["兰州西"]="LAJ";
	station["南昌"]="NCG";
	station["南京"]="NJH";
	station["南京南"]="NKH";
	station["南宁"]="NNZ";
	station["石家庄北"]="VVP";
	station["石家庄"]="SJP";
	station["沈阳"]="SYT";
	station["沈阳北"]="SBT";
	station["沈阳东"]="SDT";
	station["太原北"]="TBV";
	station["太原东"]="TDV";
	station["太原"]="TYV";
	station["武汉"]="WHN";
	station["王家营西"]="KNM";
	station["乌鲁木齐"]="WMR";
	station["西安北"]="EAY";
	station["西安"]="XAY";
	station["西安南"]="CAY";
	station["西宁西"]="XXO";
	station["银川"]="YIJ";
	station["郑州"]="ZZF";
	station["阿尔山"]="ART";
	station["安康"]="AKY";
	station["阿克苏"]="ASR";
	station["阿里河"]="AHX";
	station["阿拉山口"]="AKR";
	station["安平"]="APT";
	station["安庆"]="AQH";
	station["安顺"]="ASW";
	station["鞍山"]="AST";
	station["安阳"]="AYF";
	station["北安"]="BAB";
	station["蚌埠"]="BBH";
	station["白城"]="BCT";
	station["北海"]="BHZ";
	station["白河"]="BEL";
	station["白涧"]="BAP";
	station["宝鸡"]="BJY";
	station["滨江"]="BJB";
	station["博克图"]="BKX";
	station["百色"]="BIZ";
	station["白山市"]="HJL";
	station["北台"]="BTT";
	station["包头东"]="BDC";
	station["包头"]="BTC";
	station["北屯市"]="BXR";
	station["本溪"]="BXT";
	station["白云鄂博"]="BEC";
	station["白银西"]="BXJ";
	station["亳州"]="BZH";
	station["赤壁"]="CBN";
	station["常德"]="VGQ";
	station["承德"]="CDP";
	station["长甸"]="CDT";
	station["赤峰"]="CFD";
	station["茶陵"]="CDG";
	station["苍南"]="CEH";
	station["昌平"]="CPP";
	station["崇仁"]="CRG";
	station["昌图"]="CTT";
	station["长汀镇"]="CDB";
	station["崇信"]="CIJ";
	station["曹县"]="CXK";
	station["楚雄"]="COM";
	station["陈相屯"]="CXT";
	station["长治北"]="CBF";
	station["长征"]="CZJ";
	station["池州"]="IYH";
	station["常州"]="CZH";
	station["郴州"]="CZQ";
	station["长治"]="CZF";
	station["沧州"]="COP";
	station["崇左"]="CZZ";
	station["大安北"]="RNT";
	station["大成"]="DCT";
	station["丹东"]="DUT";
	station["东方红"]="DFB";
	station["东莞东"]="DMQ";
	station["大虎山"]="DHD";
	station["敦煌"]="DHJ";
	station["敦化"]="DHL";
	station["德惠"]="DHT";
	station["东京城"]="DJB";
	station["大涧"]="DFP";
	station["都江堰"]="DDW";
	station["大连北"]="DFT";
	station["大理"]="DKM";
	station["大连"]="DLT";
	station["定南"]="DNG";
	station["大庆"]="DZX";
	station["东胜"]="DOC";
	station["大石桥"]="DQT";
	station["大同"]="DTV";
	station["东营"]="DPK";
	station["大杨树"]="DUX";
	station["都匀"]="RYW";
	station["邓州"]="DOF";
	station["达州"]="RXW";
	station["德州"]="DZP";
	station["额济纳"]="EJC";
	station["二连"]="RLC";
	station["恩施"]="ESN";
	station["防城港"]="FEZ";
	station["福鼎"]="FES";
	station["风陵渡"]="FLV";
	station["涪陵"]="FLW";
	station["富拉尔基"]="FRX";
	station["抚顺北"]="FET";
	station["佛山"]="FSQ";
	station["阜新"]="FXD";
	station["阜阳"]="FYH";
	station["格尔木"]="GRO";
	station["广汉"]="GHW";
	station["古交"]="GJV";
	station["桂林北"]="GBZ";
	station["古莲"]="GRX";
	station["桂林"]="GLZ";
	station["固始"]="GXN";
	station["广水"]="GSN";
	station["干塘"]="GNJ";
	station["广元"]="GYW";
	station["赣州"]="GZG";
	station["公主岭"]="GLT";
	station["公主岭南"]="GBT";
	station["淮安"]="AUH";
	station["鹤北"]="HMB";
	station["淮北"]="HRH";
	station["淮滨"]="HVN";
	station["河边"]="HBV";
	station["潢川"]="KCN";
	station["韩城"]="HCY";
	station["邯郸"]="HDP";
	station["横道河子"]="HDB";
	station["鹤岗"]="HGB";
	station["皇姑屯"]="HTT";
	station["红果"]="HEM";
	station["黑河"]="HJB";
	station["怀化"]="HHQ";
	station["汉口"]="HKN";
	station["葫芦岛"]="HLD";
	station["海拉尔"]="HRX";
	station["霍林郭勒"]="HWD";
	station["海伦"]="HLB";
	station["侯马"]="HMV";
	station["哈密"]="HMR";
	station["淮南"]="HAH";
	station["桦南"]="HNB";
	station["海宁西"]="EUH";
	station["鹤庆"]="HQM";
	station["怀柔北"]="HBP";
	station["怀柔"]="HRP";
	station["黄石东"]="OSN";
	station["华山"]="HSY";
	station["黄石"]="HSN";
	station["黄山"]="HKH";
	station["衡水"]="HSP";
	station["衡阳"]="HYQ";
	station["菏泽"]="HIK";
	station["贺州"]="HXZ";
	station["汉中"]="HOY";
	station["惠州"]="HCQ";
	station["吉安"]="VAG";
	station["集安"]="JAL";
	station["江边村"]="JBG";
	station["晋城"]="JCF";
	station["金城江"]="JJZ";
	station["景德镇"]="JCG";
	station["嘉峰"]="JFF";
	station["加格达奇"]="JGX";
	station["井冈山"]="JGG";
	station["蛟河"]="JHL";
	station["金华南"]="RNH";
	station["金华西"]="JBH";
	station["九江"]="JJG";
	station["吉林"]="JLL";
	station["荆门"]="JMN";
	station["佳木斯"]="JMB";
	station["济宁"]="JIK";
	station["集宁南"]="JAC";
	station["酒泉"]="JQJ";
	station["江山"]="JUH";
	station["吉首"]="JIQ";
	station["九台"]="JTL";
	station["镜铁山"]="JVJ";
	station["鸡西"]="JXB";
	station["蓟县"]="JKP";
	station["绩溪县"]="JRH";
	station["嘉峪关"]="JGJ";
	station["江油"]="JFW";
	station["锦州"]="JZD";
	station["金州"]="JZT";
	station["库尔勒"]="KLR";
	station["开封"]="KFF";
	station["岢岚"]="KLV";
	station["凯里"]="KLW";
	station["喀什"]="KSR";
	station["昆山南"]="KNH";
	station["奎屯"]="KTR";
	station["开原"]="KYT";
	station["六安"]="UAH";
	station["灵宝"]="LBF";
	station["芦潮港"]="UCH";
	station["隆昌"]="LCW";
	station["陆川"]="LKZ";
	station["利川"]="LCN";
	station["临川"]="LCG";
	station["潞城"]="UTP";
	station["鹿道"]="LDL";
	station["娄底"]="LDQ";
	station["临汾"]="LFV";
	station["良各庄"]="LGP";
	station["临河"]="LHC";
	station["漯河"]="LON";
	station["绿化"]="LWJ";
	station["隆化"]="UHP";
	station["丽江"]="LHM";
	station["临江"]="LQL";
	station["龙井"]="LJL";
	station["吕梁"]="LHV";
	station["醴陵"]="LLG";
	station["滦平"]="UPP";
	station["六盘水"]="UMW";
	station["灵丘"]="LVV";
	station["旅顺"]="LST";
	station["陇西"]="LXJ";
	station["澧县"]="LEQ";
	station["兰溪"]="LWH";
	station["临西"]="UEP";
	station["耒阳"]="LYQ";
	station["洛阳"]="LYF";
	station["龙岩"]="LYS";
	station["洛阳东"]="LDF";
	station["连云港东"]="UKH";
	station["临沂"]="LVK";
	station["洛阳龙门"]="LLF";
	station["柳园"]="DHR";
	station["凌源"]="LYD";
	station["辽源"]="LYL";
	station["立志"]="LZX";
	station["柳州"]="LZZ";
	station["辽中"]="LZD";
	station["麻城"]="MCN";
	station["免渡河"]="MDX";
	station["牡丹江"]="MDB";
	station["莫尔道嘎"]="MRX";
	station["满归"]="MHX";
	station["明光"]="MGH";
	station["漠河"]="MVX";
	station["梅江"]="MKQ";
	station["茂名东"]="MDQ";
	station["茂名"]="MMZ";
	station["密山"]="MSB";
	station["马三家"]="MJT";
	station["麻尾"]="VAW";
	station["绵阳"]="MYW";
	station["梅州"]="MOQ";
	station["满洲里"]="MLX";
	station["宁波东"]="NVH";
	station["南岔"]="NCB";
	station["南充"]="NCW";
	station["南丹"]="NDZ";
	station["南大庙"]="NMP";
	station["南芬"]="NFT";
	station["讷河"]="NHX";
	station["嫩江"]="NGX";
	station["内江"]="NJW";
	station["南平"]="NPS";
	station["南通"]="NUH";
	station["南阳"]="NFF";
	station["碾子山"]="NZX";
	station["平顶山"]="PEN";
	station["盘锦"]="PVD";
	station["平凉"]="PIJ";
	station["平凉南"]="POJ";
	station["平泉"]="PQP";
	station["坪石"]="PSQ";
	station["萍乡"]="PXG";
	station["凭祥"]="PXZ";
	station["郫县西"]="PCW";
	station["攀枝花"]="PRW";
	station["蕲春"]="QRN";
	station["青城山"]="QSW";
	station["青岛"]="QDK";
	station["清河城"]="QYP";
	station["黔江"]="QNW";
	station["曲靖"]="QJM";
	station["前进镇"]="QEB";
	station["齐齐哈尔"]="QHX";
	station["七台河"]="QTB";
	station["沁县"]="QVV";
	station["泉州东"]="QRS";
	station["泉州"]="QYS";
	station["衢州"]="QEH";
	station["融安"]="RAZ";
	station["汝箕沟"]="RQJ";
	station["瑞金"]="RJG";
	station["日照"]="RZK";
	station["双城堡"]="SCB";
	station["绥芬河"]="SFB";
	station["韶关东"]="SGQ";
	station["山海关"]="SHD";
	station["绥化"]="SHB";
	station["三间房"]="SFX";
	station["苏家屯"]="SXT";
	station["舒兰"]="SLL";
	station["三明"]="SMS";
	station["神木"]="OMY";
	station["三门峡"]="SMF";
	station["商南"]="ONY";
	station["遂宁"]="NIW";
	station["四平"]="SPT";
	station["商丘"]="SQF";
	station["上饶"]="SRG";
	station["韶山"]="SSQ";
	station["宿松"]="OAH";
	station["汕头"]="OTQ";
	station["邵武"]="SWS";
	station["涉县"]="OEP";
	station["三亚"]="SEQ";
	station["邵阳"]="SYQ";
	station["十堰"]="SNN";
	station["双鸭山"]="SSB";
	station["松原"]="VYT";
	station["深圳"]="SZQ";
	station["苏州"]="SZH";
	station["随州"]="SZN";
	station["宿州"]="OXH";
	station["朔州"]="SUV";
	station["深圳西"]="OSQ";
	station["塘豹"]="TBQ";
	station["塔尔气"]="TVX";
	station["潼关"]="TGY";
	station["塘沽"]="TGP";
	station["塔河"]="TXX";
	station["通化"]="THL";
	station["泰来"]="TLX";
	station["吐鲁番"]="TFR";
	station["通辽"]="TLD";
	station["铁岭"]="TLT";
	station["陶赖昭"]="TPT";
	station["图们"]="TML";
	station["铜仁"]="RDQ";
	station["唐山北"]="FUP";
	station["田师府"]="TFT";
	station["泰山"]="TAK";
	station["天水"]="TSJ";
	station["唐山"]="TSP";
	station["通远堡"]="TYT";
	station["太阳升"]="TQT";
	station["泰州"]="UTH";
	station["桐梓"]="TZW";
	station["通州西"]="TAP";
	station["五常"]="WCB";
	station["武昌"]="WCN";
	station["瓦房店"]="WDT";
	station["威海"]="WKK";
	station["芜湖"]="WHH";
	station["乌海西"]="WXC";
	station["吴家屯"]="WJT";
	station["武隆"]="WLW";
	station["乌兰浩特"]="WWT";
	station["渭南"]="WNY";
	station["威舍"]="WSM";
	station["歪头山"]="WIT";
	station["武威"]="WUJ";
	station["武威南"]="WWJ";
	station["无锡"]="WXH";
	station["乌西"]="WXR";
	station["乌伊岭"]="WPB";
	station["武夷山"]="WAS";
	station["万源"]="WYY";
	station["万州"]="WYW";
	station["梧州"]="WZZ";
	station["温州"]="RZH";
	station["温州南"]="VRH";
	station["西昌"]="ECW";
	station["许昌"]="XCF";
	station["西昌南"]="ENW";
	station["香坊"]="XFB";
	station["轩岗"]="XGV";
	station["兴国"]="EUG";
	station["宣汉"]="XHY";
	station["新会"]="EFQ";
	station["新晃"]="XLQ";
	station["锡林浩特"]="XTC";
	station["兴隆县"]="EXP";
	station["厦门北"]="XKS";
	station["厦门"]="XMS";
	station["厦门高崎"]="XBS";
	station["秀山"]="ETW";
	station["小市"]="XST";
	station["向塘"]="XTG";
	station["宣威"]="XWM";
	station["新乡"]="XXF";
	station["信阳"]="XUN";
	station["咸阳"]="XYY";
	station["襄阳"]="XFN";
	station["熊岳城"]="XYT";
	station["兴义"]="XRZ";
	station["新沂"]="VIH";
	station["新余"]="XUG";
	station["徐州"]="XCH";
	station["延安"]="YWY";
	station["宜宾"]="YBW";
	station["亚布力南"]="YWB";
	station["叶柏寿"]="YBD";
	station["宜昌东"]="HAN";
	station["永川"]="YCW";
	station["宜春"]="YCG";
	station["宜昌"]="YCN";
	station["盐城"]="AFH";
	station["运城"]="YNV";
	station["伊春"]="YCB";
	station["榆次"]="YCV";
	station["杨村"]="YBP";
	station["燕岗"]="YGW";
	station["永济"]="YIV";
	station["延吉"]="YJL";
	station["营口"]="YKT";
	station["牙克石"]="YKX";
	station["阎良"]="YNY";
	station["玉林"]="YLZ";
	station["榆林"]="ALY";
	station["一面坡"]="YPB";
	station["伊宁"]="YMR";
	station["阳平关"]="YAY";
	station["玉屏"]="YZW";
	station["原平"]="YPV";
	station["延庆"]="YNP";
	station["阳泉曲"]="YYV";
	station["玉泉"]="YQB";
	station["阳泉"]="AQP";
	station["玉山"]="YNG";
	station["营山"]="NUW";
	station["燕山"]="AOP";
	station["榆树"]="YRT";
	station["鹰潭"]="YTG";
	station["烟台"]="YAK";
	station["伊图里河"]="YEX";
	station["玉田县"]="ATP";
	station["义乌"]="YWH";
	station["阳新"]="YON";
	station["义县"]="YXD";
	station["益阳"]="AEQ";
	station["岳阳"]="YYQ";
	station["永州"]="AOQ";
	station["扬州"]="YLH";
	station["淄博"]="ZBK";
	station["镇城底"]="ZDV";
	station["自贡"]="ZGW";
	station["珠海"]="ZHQ";
	station["珠海北"]="ZIQ";
	station["湛江"]="ZJZ";
	station["镇江"]="ZJH";
	station["张家界"]="DIQ";
	station["张家口"]="ZKP";
	station["张家口南"]="ZMP";
	station["周口"]="ZKN";
	station["哲里木"]="ZLC";
	station["扎兰屯"]="ZTX";
	station["驻马店"]="ZDN";
	station["肇庆"]="ZVQ";
	station["周水子"]="ZIT";
	station["昭通"]="ZDW";
	station["中卫"]="ZWJ";
	station["资阳"]="ZYW";
	station["遵义"]="ZIW";
	station["枣庄"]="ZEK";
	station["资中"]="ZZW";
	station["株洲"]="ZZQ";
	station["枣庄西"]="ZFK";
	station["昂昂溪"]="AAX";
	station["阿城"]="ACB";
	station["安达"]="ADX";
	station["安定"]="ADP";
	station["安广"]="AGT";
	station["艾河"]="AHP";
	station["安化"]="PKQ";
	station["艾家村"]="AJJ";
	station["鳌江"]="ARH";
	station["安家"]="AJB";
	station["阿金"]="AJD";
	station["阿克陶"]="AER";
	station["安口窑"]="AYY";
	station["敖力布告"]="ALD";
	station["安龙"]="AUZ";
	station["阿龙山"]="ASX";
	station["安陆"]="ALN";
	station["阿木尔"]="JTX";
	station["阿南庄"]="AZM";
	station["安庆西"]="APH";
	station["鞍山西"]="AXT";
	station["安塘"]="ATV";
	station["安亭北"]="ASH";
	station["阿图什"]="ATR";
	station["安图"]="ATL";
	station["安溪"]="AXS";
	station["博鳌"]="BWQ";
	station["白壁关"]="BGV";
	station["蚌埠南"]="BMH";
	station["巴楚"]="BCR";
	station["板城"]="BUP";
	station["北戴河"]="BEP";
	station["保定"]="BDP";
	station["宝坻"]="BPP";
	station["八达岭"]="ILP";
	station["巴东"]="BNN";
	station["柏果"]="BGM";
	station["布海"]="BUT";
	station["白河东"]="BIY";
	station["贲红"]="BVC";
	station["宝华山"]="BWH";
	station["白河县"]="BEY";
	station["白芨沟"]="BJJ";
	station["碧鸡关"]="BJM";
	station["北滘"]="IBQ";
	station["白鸡坡"]="BBM";
	station["笔架山"]="BSB";
	station["八角台"]="BTD";
	station["保康"]="BKD";
	station["白奎堡"]="BKB";
	station["白狼"]="BAT";
	station["百浪"]="BRZ";
	station["博乐"]="BOR";
	station["宝拉格"]="BQC";
	station["巴林"]="BLX";
	station["宝林"]="BNB";
	station["北流"]="BOZ";
	station["勃利"]="BLB";
	station["布列开"]="BLR";
	station["宝龙山"]="BND";
	station["八面城"]="BMD";
	station["班猫箐"]="BNM";
	station["八面通"]="BMB";
	station["北马圈子"]="BRP";
	station["北票南"]="RPD";
	station["白旗"]="BQP";
	station["宝泉岭"]="BQB";
	station["白沙"]="BSW";
	station["巴山"]="BAY";
	station["白水江"]="BSY";
	station["白沙坡"]="BPM";
	station["白石山"]="BAL";
	station["白水镇"]="BUM";
	station["坂田"]="BTQ";
	station["泊头"]="BZP";
	station["北屯"]="BYP";
	station["博兴"]="BXK";
	station["八仙筒"]="VXD";
	station["白音察干"]="BYC";
	station["背荫河"]="BYB";
	station["北营"]="BIV";
	station["巴彦高勒"]="BAC";
	station["白音他拉"]="BID";
	station["鲅鱼圈"]="BYT";
	station["白银市"]="BNJ";
	station["白音胡硕"]="BCD";
	station["巴中"]="IEW";
	station["霸州"]="RMP";
	station["北宅"]="BVP";
	station["赤壁北"]="CIN";
	station["查布嘎"]="CBC";
	station["长城"]="CEJ";
	station["长冲"]="CCM";
	station["承德东"]="CCP";
	station["赤峰西"]="CID";
	station["嵯岗"]="CAX";
	station["长葛"]="CEF";
	station["柴沟堡"]="CGV";
	station["城固"]="CGY";
	station["陈官营"]="CAJ";
	station["成高子"]="CZB";
	station["草海"]="WBW";
	station["柴河"]="CHB";
	station["册亨"]="CHZ";
	station["草河口"]="CKT";
	station["崔黄口"]="CHP";
	station["巢湖"]="CIH";
	station["蔡家沟"]="CJT";
	station["成吉思汗"]="CJX";
	station["岔江"]="CAM";
	station["蔡家坡"]="CJY";
	station["沧口"]="CKK";
	station["昌乐"]="CLK";
	station["超梁沟"]="CYP";
	station["慈利"]="CUQ";
	station["昌黎"]="CLP";
	station["长岭子"]="CLT";
	station["晨明"]="CMB";
	station["长农"]="CNJ";
	station["昌平北"]="VBP";
	station["长坡岭"]="CPM";
	station["辰清"]="CQB";
	station["楚山"]="CSB";
	station["长寿"]="EFW";
	station["磁山"]="CSP";
	station["苍石"]="CST";
	station["草市"]="CSL";
	station["察素齐"]="CSC";
	station["长山屯"]="CVT";
	station["长汀"]="CES";
	station["昌图西"]="CPT";
	station["春湾"]="CQQ";
	station["磁县"]="CIP";
	station["岑溪"]="CNZ";
	station["辰溪"]="CXQ";
	station["磁西"]="CRP";
	station["长兴南"]="CFH";
	station["磁窑"]="CYK";
	station["朝阳"]="CYD";
	station["春阳"]="CAL";
	station["城阳"]="CEK";
	station["创业村"]="CEX";
	station["朝阳川"]="CYL";
	station["朝阳地"]="CDD";
	station["长垣"]="CYF";
	station["朝阳镇"]="CZL";
	station["滁州北"]="CUH";
	station["常州北"]="ESH";
	station["滁州"]="CXH";
	station["潮州"]="CKQ";
	station["常庄"]="CVK";
	station["曹子里"]="CFP";
	station["车转湾"]="CWM";
	station["郴州西"]="ICQ";
	station["沧州西"]="CBP";
	station["德安"]="DAG";
	station["东安"]="DAZ";
	station["大坝"]="DBJ";
	station["大板"]="DBC";
	station["大巴"]="DBD";
	station["到保"]="RBT";
	station["定边"]="DYJ";
	station["东边井"]="DBB";
	station["德伯斯"]="RDT";
	station["打柴沟"]="DGJ";
	station["德昌"]="DVW";
	station["滴道"]="DDB";
	station["大德"]="DEM";
	station["大磴沟"]="DKJ";
	station["刀尔登"]="DRD";
	station["得耳布尔"]="DRX";
	station["东方"]="UFQ";
	station["丹凤"]="DGY";
	station["东丰"]="DIL";
	station["都格"]="DMM";
	station["大官屯"]="DTT";
	station["大关"]="RGW";
	station["东光"]="DGP";
	station["东莞"]="DAQ";
	station["东海"]="DHB";
	station["大灰厂"]="DHP";
	station["大红旗"]="DQD";
	station["东海县"]="DQH";
	station["德惠西"]="DXT";
	station["达家沟"]="DJT";
	station["东津"]="DKB";
	station["杜家"]="DJL";
	station["大旧庄"]="DJM";
	station["大口屯"]="DKP";
	station["东来"]="RVD";
	station["德令哈"]="DHO";
	station["大陆号"]="DLC";
	station["带岭"]="DLB";
	station["大林"]="DLD";
	station["达拉特旗"]="DIC";
	station["独立屯"]="DTX";
	station["达拉特西"]="DNC";
	station["东明村"]="DMD";
	station["洞庙河"]="DEP";
	station["东明县"]="DNF";
	station["大拟"]="DNZ";
	station["大平房"]="DPD";
	station["大盘石"]="RPP";
	station["大埔"]="DPI";
	station["大堡"]="DVT";
	station["大其拉哈"]="DQX";
	station["道清"]="DML";
	station["对青山"]="DQB";
	station["德清西"]="MOH";
	station["东升"]="DRQ";
	station["独山"]="RWW";
	station["砀山"]="DKH";
	station["登沙河"]="DWT";
	station["读书铺"]="DPM";
	station["大石头"]="DSL";
	station["大石寨"]="RZT";
	station["东台"]="DBH";
	station["定陶"]="DQK";
	station["灯塔"]="DGT";
	station["大田边"]="DBM";
	station["东通化"]="DTL";
	station["丹徒"]="RUH";
	station["大屯"]="DNT";
	station["东湾"]="DRJ";
	station["大武口"]="DFJ";
	station["低窝铺"]="DWJ";
	station["大王滩"]="DZZ";
	station["大湾子"]="DFM";
	station["大兴沟"]="DXL";
	station["大兴"]="DXX";
	station["定西"]="DSJ";
	station["甸心"]="DXM";
	station["东乡"]="DXG";
	station["代县"]="DKV";
	station["定襄"]="DXV";
	station["东戌"]="RXP";
	station["东辛庄"]="DXD";
	station["丹阳"]="DYH";
	station["大雁"]="DYX";
	station["德阳"]="DYW";
	station["当阳"]="DYN";
	station["丹阳北"]="EXH";
	station["大英东"]="IAW";
	station["东淤地"]="DBV";
	station["大营"]="DYV";
	station["定远"]="EWH";
	station["岱岳"]="RYV";
	station["大元"]="DYZ";
	station["大营镇"]="DJP";
	station["大营子"]="DZD";
	station["大战场"]="DTJ";
	station["德州东"]="DIP";
	station["低庄"]="DVQ";
	station["东镇"]="DNV";
	station["道州"]="DFZ";
	station["东至"]="DCH";
	station["兑镇"]="DWV";
	station["豆庄"]="ROP";
	station["定州"]="DXP";
	station["大竹园"]="DZY";
	station["大杖子"]="DAP";
	station["豆张庄"]="RZP";
	station["峨边"]="EBW";
	station["二道沟门"]="RDP";
	station["二道湾"]="RDX";
	station["二龙"]="RLD";
	station["二龙山屯"]="ELA";
	station["峨眉"]="EMW";
	station["二营"]="RYJ";
	station["鄂州"]="ECN";
	station["福安"]="FAS";
	station["防城"]="FAZ";
	station["丰城"]="FCG";
	station["丰城南"]="FNG";
	station["肥东"]="FIH";
	station["发耳"]="FEM";
	station["富海"]="FHX";
	station["福海"]="FHR";
	station["凤凰城"]="FHT";
	station["奉化"]="FHH";
	station["富锦"]="FIB";
	station["范家屯"]="FTT";
	station["福利屯"]="FTB";
	station["丰乐镇"]="FZB";
	station["阜南"]="FNH";
	station["阜宁"]="AKH";
	station["抚宁"]="FNP";
	station["福清"]="FQS";
	station["福泉"]="VMW";
	station["丰水村"]="FSJ";
	station["丰顺"]="FUQ";
	station["繁峙"]="FSV";
	station["抚顺"]="FST";
	station["福山口"]="FKP";
	station["扶绥"]="FSZ";
	station["冯屯"]="FTX";
	station["浮图峪"]="FYP";
	station["富县东"]="FDY";
	station["凤县"]="FXY";
	station["富县"]="FEY";
	station["费县"]="FXK";
	station["凤阳"]="FUH";
	station["汾阳"]="FAV";
	station["扶余北"]="FBT";
	station["分宜"]="FYG";
	station["富源"]="FYM";
	station["扶余"]="FYT";
	station["富裕"]="FYX";
	station["抚州北"]="FBG";
	station["凤州"]="FZY";
	station["丰镇"]="FZC";
	station["范镇"]="VZK";
	station["固安"]="GFP";
	station["广安"]="VJW";
	station["高碑店"]="GBP";
	station["沟帮子"]="GBD";
	station["甘草店"]="GDJ";
	station["谷城"]="GCN";
	station["藁城"]="GEP";
	station["古城镇"]="GZB";
	station["广德"]="GRH";
	station["贵定"]="GTW";
	station["贵定南"]="IDW";
	station["古东"]="GDV";
	station["贵港"]="GGZ";
	station["官高"]="GVP";
	station["葛根庙"]="GGT";
	station["甘谷"]="GGJ";
	station["高各庄"]="GGP";
	station["甘河"]="GAX";
	station["根河"]="GEX";
	station["郭家店"]="GDT";
	station["孤家子"]="GKT";
	station["高老"]="GOB";
	station["古浪"]="GLJ";
	station["皋兰"]="GEJ";
	station["高楼房"]="GFM";
	station["归流河"]="GHT";
	station["关林"]="GLF";
	station["甘洛"]="VOW";
	station["郭磊庄"]="GLP";
	station["高密"]="GMK";
	station["公庙子"]="GMC";
	station["工农湖"]="GRT";
	station["广宁寺"]="GNT";
	station["广南卫"]="GNM";
	station["高平"]="GPF";
	station["甘泉北"]="GEY";
	station["共青城"]="GAG";
	station["甘旗卡"]="GQD";
	station["甘泉"]="GQY";
	station["高桥镇"]="GZD";
	station["赶水"]="GSW";
	station["灌水"]="GST";
	station["孤山口"]="GSP";
	station["果松"]="GSL";
	station["高山子"]="GSD";
	station["嘎什甸子"]="GXD";
	station["高台"]="GTJ";
	station["高滩"]="GAY";
	station["古田"]="GTS";
	station["官厅"]="GTP";
	station["广通"]="GOM";
	station["官厅西"]="KEP";
	station["贵溪"]="GXG";
	station["涡阳"]="GYH";
	station["巩义"]="GXF";
	station["高邑"]="GIP";
	station["巩义南"]="GYF";
	station["固原"]="GUJ";
	station["菇园"]="GYL";
	station["公营子"]="GYD";
	station["光泽"]="GZS";
	station["古镇"]="GNQ";
	station["瓜州"]="GZJ";
	station["高州"]="GSQ";
	station["固镇"]="GEH";
	station["盖州"]="GXT";
	station["官字井"]="GOT";
	station["革镇堡"]="GZT";
	station["冠豸山"]="GSS";
	station["盖州西"]="GAT";
	station["红安"]="HWN";
	station["淮安南"]="AMH";
	station["红安西"]="VXN";
	station["海安县"]="HIH";
	station["黄柏"]="HBL";
	station["海北"]="HEB";
	station["鹤壁"]="HAF";
	station["华城"]="VCQ";
	station["合川"]="WKW";
	station["河唇"]="HCZ";
	station["汉川"]="HCN";
	station["海城"]="HCT";
	station["黑冲滩"]="HCJ";
	station["黄村"]="HCP";
	station["海城西"]="HXT";
	station["化德"]="HGC";
	station["洪洞"]="HDV";
	station["横峰"]="HFG";
	station["韩府湾"]="HXJ";
	station["汉沽"]="HGP";
	station["黄瓜园"]="HYM";
	station["红光镇"]="IGW";
	station["红花沟"]="VHD";
	station["黄花筒"]="HUD";
	station["贺家店"]="HJJ";
	station["和静"]="HJR";
	station["红江"]="HFM";
	station["黑井"]="HIM";
	station["获嘉"]="HJF";
	station["河津"]="HJV";
	station["涵江"]="HJS";
	station["河间西"]="HXP";
	station["花家庄"]="HJM";
	station["河口南"]="HKJ";
	station["黄口"]="KOH";
	station["湖口"]="HKG";
	station["呼兰"]="HUB";
	station["葫芦岛北"]="HPD";
	station["浩良河"]="HHB";
	station["哈拉海"]="HIT";
	station["鹤立"]="HOB";
	station["桦林"]="HIB";
	station["黄陵"]="ULY";
	station["海林"]="HRB";
	station["虎林"]="VLB";
	station["寒岭"]="HAT";
	station["和龙"]="HLL";
	station["海龙"]="HIL";
	station["哈拉苏"]="HAX";
	station["呼鲁斯太"]="VTJ";
	station["黄梅"]="VEH";
	station["蛤蟆塘"]="HMT";
	station["韩麻营"]="HYP";
	station["黄泥河"]="HHL";
	station["海宁"]="HNH";
	station["惠农"]="HMJ";
	station["和平"]="VAQ";
	station["花棚子"]="HZM";
	station["花桥"]="VQH";
	station["宏庆"]="HEY";
	station["怀仁"]="HRV";
	station["华容"]="HRN";
	station["华山北"]="HDY";
	station["黄松甸"]="HDL";
	station["和什托洛盖"]="VSR";
	station["红山"]="VSB";
	station["汉寿"]="VSQ";
	station["衡山"]="HSQ";
	station["黑水"]="HOT";
	station["惠山"]="VCH";
	station["虎什哈"]="HHP";
	station["红寺堡"]="HSJ";
	station["海石湾"]="HSO";
	station["衡山西"]="HEQ";
	station["红砂岘"]="VSJ";
	station["黑台"]="HQB";
	station["桓台"]="VTK";
	station["和田"]="VTR";
	station["会同"]="VTQ";
	station["海坨子"]="HZT";
	station["黑旺"]="HWK";
	station["海湾"]="RWH";
	station["红星"]="VXB";
	station["徽县"]="HYY";
	station["红兴隆"]="VHB";
	station["换新天"]="VTB";
	station["红岘台"]="HTJ";
	station["红彦"]="VIX";
	station["合阳"]="HAY";
	station["海阳"]="HYK";
	station["衡阳东"]="HVQ";
	station["华蓥"]="HUW";
	station["汉阴"]="HQY";
	station["黄羊滩"]="HGJ";
	station["汉源"]="WHW";
	station["河源"]="VIQ";
	station["花园"]="HUN";
	station["黄羊镇"]="HYJ";
	station["化州"]="HZZ";
	station["黄州"]="VON";
	station["霍州"]="HZV";
	station["惠州西"]="VXQ";
	station["巨宝"]="JRT";
	station["靖边"]="JIY";
	station["金宝屯"]="JBD";
	station["晋城北"]="JEF";
	station["金昌"]="JCJ";
	station["鄄城"]="JCK";
	station["交城"]="JNV";
	station["建昌"]="JFD";
	station["峻德"]="JDB";
	station["井店"]="JFP";
	station["鸡东"]="JOB";
	station["江都"]="UDH";
	station["鸡冠山"]="JST";
	station["金沟屯"]="VGP";
	station["静海"]="JHP";
	station["金河"]="JHX";
	station["锦河"]="JHB";
	station["锦和"]="JHQ";
	station["精河"]="JHR";
	station["精河南"]="JIR";
	station["江华"]="JHZ";
	station["建湖"]="AJH";
	station["纪家沟"]="VJD";
	station["晋江"]="JJS";
	station["江津"]="JJW";
	station["姜家"]="JJB";
	station["金坑"]="JKT";
	station["芨岭"]="JLJ";
	station["金马村"]="JMM";
	station["角美"]="JES";
	station["江门"]="JWQ";
	station["莒南"]="JOK";
	station["井南"]="JNP";
	station["建瓯"]="JVS";
	station["经棚"]="JPC";
	station["江桥"]="JQX";
	station["九三"]="SSX";
	station["金山北"]="EGH";
	station["京山"]="JCN";
	station["建始"]="JRN";
	station["嘉善"]="JSH";
	station["稷山"]="JVV";
	station["吉舒"]="JSL";
	station["建设"]="JET";
	station["甲山"]="JOP";
	station["建三江"]="JIB";
	station["嘉善南"]="EAH";
	station["金山屯"]="JTB";
	station["江所田"]="JOM";
	station["景泰"]="JTJ";
	station["吉文"]="JWX";
	station["进贤"]="JUG";
	station["莒县"]="JKK";
	station["嘉祥"]="JUK";
	station["介休"]="JXV";
	station["井陉"]="JJP";
	station["嘉兴"]="JXH";
	station["嘉兴南"]="EPH";
	station["夹心子"]="JXT";
	station["简阳"]="JYW";
	station["揭阳"]="JRQ";
	station["建阳"]="JYS";
	station["姜堰"]="UEH";
	station["巨野"]="JYK";
	station["江永"]="JYZ";
	station["靖远"]="JYJ";
	station["缙云"]="JYH";
	station["江源"]="SZL";
	station["济源"]="JYF";
	station["靖远西"]="JXJ";
	station["胶州北"]="JZK";
	station["焦作东"]="WEF";
	station["靖州"]="JEQ";
	station["荆州"]="JBN";
	station["金寨"]="JZH";
	station["晋州"]="JXP";
	station["胶州"]="JXK";
	station["锦州南"]="JOD";
	station["焦作"]="JOF";
	station["旧庄窝"]="JVP";
	station["金杖子"]="JYD";
	station["开安"]="KAT";
	station["库车"]="KCR";
	station["康城"]="KCP";
	station["库都尔"]="KDX";
	station["宽甸"]="KDT";
	station["克东"]="KOB";
	station["开江"]="KAW";
	station["康金井"]="KJB";
	station["喀喇其"]="KQX";
	station["开鲁"]="KLC";
	station["克拉玛依"]="KHR";
	station["口前"]="KQL";
	station["奎山"]="KAB";
	station["昆山"]="KSH";
	station["克山"]="KSB";
	station["开通"]="KTT";
	station["康熙岭"]="KXZ";
	station["克一河"]="KHX";
	station["开原西"]="KXT";
	station["康庄"]="KZP";
	station["来宾"]="UBZ";
	station["老边"]="LLT";
	station["灵宝西"]="LPF";
	station["龙川"]="LUQ";
	station["乐昌"]="LCQ";
	station["黎城"]="UCP";
	station["聊城"]="UCK";
	station["蓝村"]="LCK";
	station["林东"]="LRC";
	station["乐都"]="LDO";
	station["梁底下"]="LDP";
	station["六道河子"]="LVP";
	station["鲁番"]="LVM";
	station["廊坊"]="LJP";
	station["落垡"]="LOP";
	station["廊坊北"]="LFP";
	station["龙凤"]="LFX";
	station["禄丰"]="LFM";
	station["老府"]="UFD";
	station["兰岗"]="LNB";
	station["龙骨甸"]="LGM";
	station["芦沟"]="LOM";
	station["龙沟"]="LGJ";
	station["拉古"]="LGB";
	station["临海"]="UFH";
	station["林海"]="LXX";
	station["拉哈"]="LHX";
	station["凌海"]="JID";
	station["柳河"]="LNL";
	station["六合"]="KLH";
	station["龙华"]="LHP";
	station["滦河沿"]="UNP";
	station["六合镇"]="LEX";
	station["亮甲店"]="LRT";
	station["刘家店"]="UDT";
	station["刘家河"]="LVT";
	station["连江"]="LKS";
	station["李家"]="LJB";
	station["罗江"]="LJW";
	station["廉江"]="LJZ";
	station["庐江"]="UJH";
	station["励家"]="LID";
	station["两家"]="UJT";
	station["龙江"]="LJX";
	station["龙嘉"]="UJL";
	station["莲江口"]="LHB";
	station["蔺家楼"]="ULK";
	station["李家坪"]="LIJ";
	station["兰考"]="LKF";
	station["林口"]="LKB";
	station["路口铺"]="LKQ";
	station["老莱"]="LAX";
	station["拉林"]="LAB";
	station["陆良"]="LRM";
	station["龙里"]="LLW";
	station["零陵"]="UWZ";
	station["临澧"]="LWQ";
	station["兰棱"]="LLB";
	station["卢龙"]="UAP";
	station["喇嘛甸"]="LMX";
	station["里木店"]="LMB";
	station["洛门"]="LMJ";
	station["龙门河"]="MHA";
	station["龙南"]="UNG";
	station["梁平"]="UQW";
	station["罗平"]="LPM";
	station["落坡岭"]="LPP";
	station["六盘山"]="UPJ";
	station["乐平市"]="LPG";
	station["临清"]="UQK";
	station["龙泉寺"]="UQJ";
	station["乐善村"]="LUM";
	station["冷水江东"]="UDQ";
	station["连山关"]="LGT";
	station["流水沟"]="USP";
	station["陵水"]="LIQ";
	station["乐山"]="UTW";
	station["罗山"]="LRN";
	station["鲁山"]="LAF";
	station["丽水"]="USH";
	station["梁山"]="LMK";
	station["灵石"]="LSV";
	station["露水河"]="LUL";
	station["庐山"]="LSG";
	station["林盛堡"]="LBT";
	station["柳树屯"]="LSD";
	station["梨树镇"]="LSB";
	station["龙山镇"]="LAS";
	station["李石寨"]="LET";
	station["黎塘"]="LTZ";
	station["轮台"]="LAR";
	station["芦台"]="LTP";
	station["龙塘坝"]="LBM";
	station["濑湍"]="LVZ";
	station["骆驼巷"]="LTJ";
	station["李旺"]="VLJ";
	station["莱芜东"]="LWK";
	station["狼尾山"]="LRJ";
	station["灵武"]="LNJ";
	station["莱芜西"]="UXK";
	station["朗乡"]="LXB";
	station["陇县"]="LXY";
	station["临湘"]="LXQ";
	station["莱西"]="LXK";
	station["林西"]="LXC";
	station["滦县"]="UXP";
	station["略阳"]="LYY";
	station["莱阳"]="LYK";
	station["辽阳"]="LYT";
	station["临沂北"]="UYK";
	station["凌源东"]="LDD";
	station["连云港"]="UIH";
	station["老羊壕"]="LYC";
	station["临颍"]="LNF";
	station["老营"]="LXL";
	station["龙游"]="LMH";
	station["罗源"]="LVS";
	station["林源"]="LYX";
	station["涟源"]="LAQ";
	station["涞源"]="LYP";
	station["耒阳西"]="LPQ";
	station["临泽"]="LEJ";
	station["龙爪沟"]="LZT";
	station["雷州"]="UAQ";
	station["六枝"]="LIW";
	station["鹿寨"]="LIZ";
	station["来舟"]="LZS";
	station["龙镇"]="LZA";
	station["拉鲊"]="LEM";
	station["明安"]="MAC";
	station["马鞍山"]="MAH";
	station["毛坝"]="MBY";
	station["毛坝关"]="MGY";
	station["麻城北"]="MBN";
	station["渑池"]="MCF";
	station["明城"]="MCL";
	station["庙城"]="MAP";
	station["渑池南"]="MNF";
	station["茅草坪"]="KPM";
	station["猛洞河"]="MUQ";
	station["磨刀石"]="MOB";
	station["弥渡"]="MDF";
	station["帽儿山"]="MRB";
	station["明港"]="MGN";
	station["梅河口"]="MHL";
	station["马皇"]="MHZ";
	station["孟家岗"]="MGB";
	station["美兰"]="MHQ";
	station["汨罗东"]="MQQ";
	station["马莲河"]="MHB";
	station["茅岭"]="MLZ";
	station["庙岭"]="MLL";
	station["穆棱"]="MLB";
	station["马林"]="MID";
	station["马龙"]="MGM";
	station["汨罗"]="MLQ";
	station["木里图"]="MUD";
	station["密马龙"]="MMM";
	station["玛纳斯湖"]="MNR";
	station["冕宁"]="UGW";
	station["沐滂"]="MPQ";
	station["马桥河"]="MQB";
	station["闽清"]="MQS";
	station["民权"]="MQF";
	station["明水河"]="MUT";
	station["麻山"]="MAB";
	station["眉山"]="MSW";
	station["漫水湾"]="MKW";
	station["茂舍祖"]="MOM";
	station["米沙子"]="MST";
	station["庙台子"]="MZB";
	station["美溪"]="MEB";
	station["勉县"]="MVY";
	station["麻阳"]="MVQ";
	station["牧羊村"]="MCM";
	station["米易"]="MMW";
	station["麦园"]="MYS";
	station["墨玉"]="MUR";
	station["密云"]="MUP";
	station["庙庄"]="MZJ";
	station["米脂"]="MEY";
	station["明珠"]="MFQ";
	station["宁安"]="NAB";
	station["农安"]="NAT";
	station["南博山"]="NBK";
	station["南仇"]="NCK";
	station["南城司"]="NSP";
	station["宁村"]="NCZ";
	station["宁德"]="NES";
	station["南观村"]="NGP";
	station["南宫东"]="NFP";
	station["南关岭"]="NLT";
	station["宁国"]="NNH";
	station["宁海"]="NHH";
	station["南河川"]="NHJ";
	station["南华"]="NHS";
	station["宁家"]="NVT";
	station["牛家"]="NJB";
	station["南靖"]="NJS";
	station["能家"]="NJD";
	station["南口"]="NKP";
	station["南口前"]="NKT";
	station["南朗"]="NNQ";
	station["乃林"]="NLD";
	station["尼勒克"]="NIR";
	station["那罗"]="ULZ";
	station["宁陵县"]="NLF";
	station["奈曼"]="NMD";
	station["宁明"]="NMZ";
	station["南木"]="NMX";
	station["南平南"]="NNS";
	station["那铺"]="NPZ";
	station["南桥"]="NQD";
	station["那曲"]="NQO";
	station["暖泉"]="NQJ";
	station["南台"]="NTT";
	station["南头"]="NOQ";
	station["宁武"]="NWV";
	station["南湾子"]="NWP";
	station["南翔北"]="NEH";
	station["宁乡"]="NXQ";
	station["内乡"]="NXF";
	station["牛心台"]="NXT";
	station["南峪"]="NUP";
	station["娘子关"]="NIP";
	station["南召"]="NAF";
	station["南杂木"]="NZT";
	station["平安"]="PAL";
	station["蓬安"]="PAW";
	station["平安驿"]="PNO";
	station["磐安镇"]="PAJ";
	station["平安镇"]="PZT";
	station["蒲城东"]="PEY";
	station["蒲城"]="PCY";
	station["裴德"]="PDB";
	station["偏店"]="PRP";
	station["平顶山西"]="BFF";
	station["坡底下"]="PXJ";
	station["瓢儿屯"]="PRT";
	station["平房"]="PFB";
	station["平关"]="PGM";
	station["盘关"]="PAM";
	station["平果"]="PGZ";
	station["徘徊北"]="PHP";
	station["平河口"]="PHM";
	station["盘锦北"]="PBD";
	station["潘家店"]="PDP";
	station["皮口"]="PKT";
	station["普兰店"]="PLT";
	station["偏岭"]="PNT";
	station["平山"]="PSB";
	station["彭山"]="PSW";
	station["皮山"]="PSR";
	station["彭水"]="PHW";
	station["磐石"]="PSL";
	station["平社"]="PSV";
	station["平台"]="PVT";
	station["平田"]="PTM";
	station["莆田"]="PTS";
	station["葡萄菁"]="PTW";
	station["平旺"]="PWV";
	station["普湾"]="PWT";
	station["普雄"]="POW";
	station["平洋"]="PYX";
	station["彭阳"]="PYJ";
	station["平遥"]="PYV";
	station["平邑"]="PIK";
	station["平原堡"]="PPJ";
	station["平原"]="PYK";
	station["平峪"]="PYP";
	station["彭泽"]="PZG";
	station["邳州"]="PJH";
	station["平庄"]="PZD";
	station["泡子"]="POD";
	station["平庄南"]="PND";
	station["乾安"]="QOT";
	station["庆安"]="QAB";
	station["迁安"]="QQP";
	station["祁东北"]="QRQ";
	station["七甸"]="QDM";
	station["曲阜东"]="QAK";
	station["庆丰"]="QFT";
	station["奇峰塔"]="QVP";
	station["曲阜"]="QFK";
	station["勤丰营"]="QFM";
	station["琼海"]="QYQ";
	station["秦皇岛"]="QTP";
	station["千河"]="QUY";
	station["清河"]="QIP";
	station["清河门"]="QHD";
	station["清华园"]="QHP";
	station["渠旧"]="QJZ";
	station["綦江"]="QJW";
	station["潜江"]="QJN";
	station["全椒"]="INH";
	station["秦家"]="QJB";
	station["祁家堡"]="QBT";
	station["清涧县"]="QNY";
	station["秦家庄"]="QZV";
	station["七里河"]="QLD";
	station["渠黎"]="QLZ";
	station["秦岭"]="QLY";
	station["青龙山"]="QGH";
	station["青龙寺"]="QSM";
	station["祁门"]="QIH";
	station["前磨头"]="QMP";
	station["青山"]="QSB";
	station["全胜"]="QVB";
	station["确山"]="QSN";
	station["清水"]="QUJ";
	station["前山"]="QXQ";
	station["戚墅堰"]="QYH";
	station["青田"]="QVH";
	station["桥头"]="QAT";
	station["青铜峡"]="QTJ";
	station["前苇塘"]="QWP";
	station["渠县"]="QRW";
	station["祁县"]="QXV";
	station["青县"]="QXP";
	station["桥西"]="QXJ";
	station["清徐"]="QUV";
	station["旗下营"]="QXC";
	station["千阳"]="QOY";
	station["沁阳"]="QYF";
	station["泉阳"]="QYL";
	station["祁阳北"]="QVQ";
	station["七营"]="QYJ";
	station["庆阳山"]="QSJ";
	station["清远"]="QBQ";
	station["清原"]="QYT";
	station["钦州东"]="QDZ";
	station["全州"]="QZZ";
	station["钦州"]="QRZ";
	station["青州市"]="QZK";
	station["瑞安"]="RAH";
	station["荣昌"]="RCW";
	station["瑞昌"]="RCG";
	station["如皋"]="RBH";
	station["容桂"]="RUQ";
	station["任丘"]="RQP";
	station["乳山"]="ROK";
	station["融水"]="RSZ";
	station["热水"]="RSD";
	station["容县"]="RXZ";
	station["饶阳"]="RVP";
	station["汝阳"]="RYF";
	station["绕阳河"]="RHD";
	station["汝州"]="ROF";
	station["石坝"]="OBJ";
	station["上板城"]="SBP";
	station["施秉"]="AQW";
	station["上板城南"]="OBP";
	station["双城北"]="SBB";
	station["商城"]="SWN";
	station["莎车"]="SCR";
	station["顺昌"]="SCS";
	station["舒城"]="OCH";
	station["神池"]="SMV";
	station["沙城"]="SCP";
	station["石城"]="SCT";
	station["山城镇"]="SCL";
	station["山丹"]="SDJ";
	station["顺德"]="ORQ";
	station["绥德"]="ODY";
	station["邵东"]="SOQ";
	station["水洞"]="SIL";
	station["商都"]="SXC";
	station["十渡"]="SEP";
	station["四道湾"]="OUD";
	station["绅坊"]="OLH";
	station["双丰"]="OFB";
	station["四方台"]="STB";
	station["水富"]="OTW";
	station["三关口"]="OKJ";
	station["桑根达来"]="OGC";
	station["韶关"]="SNQ";
	station["上高镇"]="SVK";
	station["上杭"]="JBS";
	station["沙海"]="SED";
	station["松河"]="SBM";
	station["沙河"]="SHP";
	station["沙河口"]="SKT";
	station["赛汗塔拉"]="SHC";
	station["沙河市"]="VOP";
	station["山河屯"]="SHL";
	station["三河县"]="OXP";
	station["四合永"]="OHD";
	station["三汇镇"]="OZW";
	station["双河镇"]="SEL";
	station["石河子"]="SZR";
	station["三合庄"]="SVP";
	station["三家店"]="ODP";
	station["水家湖"]="SQH";
	station["沈家河"]="OJJ";
	station["松江河"]="SJL";
	station["尚家"]="SJB";
	station["孙家"]="SUB";
	station["沈家"]="OJB";
	station["松江"]="SAH";
	station["三江口"]="SKD";
	station["司家岭"]="OLK";
	station["松江南"]="IMH";
	station["石景山南"]="SRP";
	station["邵家堂"]="SJJ";
	station["三江县"]="SOZ";
	station["三家寨"]="SMM";
	station["十家子"]="SJD";
	station["松江镇"]="OZL";
	station["施家嘴"]="SHM";
	station["深井子"]="SWT";
	station["什里店"]="OMP";
	station["疏勒"]="SUR";
	station["疏勒河"]="SHJ";
	station["舍力虎"]="VLD";
	station["石磷"]="SPB";
	station["绥棱"]="SIB";
	station["石岭"]="SOL";
	station["石林"]="SLM";
	station["石林南"]="LNM";
	station["石龙"]="SLQ";
	station["萨拉齐"]="SLC";
	station["索伦"]="SNT";
	station["商洛"]="OLY";
	station["沙岭子"]="SLP";
	station["三门峡南"]="SCF";
	station["三门县"]="OQH";
	station["石门县"]="OMQ";
	station["三门峡西"]="SXF";
	station["肃宁"]="SYP";
	station["宋"]="SOB";
	station["双牌"]="SBZ";
	station["四平东"]="PPT";
	station["遂平"]="SON";
	station["沙坡头"]="SFJ";
	station["商丘南"]="SPF";
	station["水泉"]="SID";
	station["石泉县"]="SXY";
	station["石桥子"]="SQT";
	station["石人城"]="SRB";
	station["石人"]="SRL";
	station["山市"]="SQB";
	station["神树"]="SWB";
	station["鄯善"]="SSR";
	station["三水"]="SJQ";
	station["泗水"]="OSK";
	station["松树"]="SFT";
	station["首山"]="SAT";
	station["三十家"]="SRD";
	station["三十里堡"]="SST";
	station["松树镇"]="SSL";
	station["松桃"]="MZQ";
	station["索图罕"]="SHX";
	station["三堂集"]="SDH";
	station["石头"]="OTB";
	station["神头"]="SEV";
	station["沙沱"]="SFM";
	station["上万"]="SWP";
	station["孙吴"]="SKB";
	station["沙湾县"]="SXR";
	station["遂溪"]="SXZ";
	station["沙县"]="SAS";
	station["绍兴"]="SOH";
	station["歙县"]="OVH";
	station["上西铺"]="SXM";
	station["石峡子"]="SXJ";
	station["绥阳"]="SYB";
	station["沭阳"]="FMH";
	station["寿阳"]="SYV";
	station["水洋"]="OYP";
	station["三阳川"]="SYJ";
	station["上腰墩"]="SPJ";
	station["三营"]="OEJ";
	station["顺义"]="SOP";
	station["三义井"]="OYD";
	station["三源浦"]="SYL";
	station["三原"]="SAY";
	station["上虞"]="BDH";
	station["上园"]="SUD";
	station["水源"]="OYJ";
	station["桑园子"]="SAJ";
	station["绥中北"]="SND";
	station["苏州北"]="OHH";
	station["宿州东"]="SRH";
	station["深圳东"]="BJQ";
	station["深州"]="OZP";
	station["孙镇"]="OZY";
	station["绥中"]="SZD";
	station["尚志"]="SZB";
	station["师庄"]="SNM";
	station["松滋"]="SIN";
	station["师宗"]="SEM";
	station["苏州园区"]="KAH";
	station["苏州新区"]="ITH";
	station["石嘴山"]="SZJ";
	station["泰安"]="TMK";
	station["台安"]="TID";
	station["通安驿"]="TAJ";
	station["桐柏"]="TBF";
	station["通北"]="TBB";
	station["汤池"]="TCX";
	station["桐城"]="TTH";
	station["郯城"]="TZK";
	station["铁厂"]="TCL";
	station["桃村"]="TCK";
	station["通道"]="TRQ";
	station["田东"]="TDZ";
	station["天岗"]="TGL";
	station["土贵乌拉"]="TGC";
	station["太谷"]="TGV";
	station["塔哈"]="THX";
	station["棠海"]="THM";
	station["唐河"]="THF";
	station["泰和"]="THG";
	station["太湖"]="TKH";
	station["团结"]="TIX";
	station["谭家井"]="TNJ";
	station["陶家屯"]="TOT";
	station["唐家湾"]="PDQ";
	station["统军庄"]="TZP";
	station["泰康"]="TKX";
	station["吐列毛杜"]="TMD";
	station["图里河"]="TEX";
	station["亭亮"]="TIZ";
	station["田林"]="TFZ";
	station["铜陵"]="TJH";
	station["铁力"]="TLB";
	station["铁岭西"]="PXT";
	station["天门"]="TMN";
	station["天门南"]="TNN";
	station["太姥山"]="TLS";
	station["土牧尔台"]="TRC";
	station["土门子"]="TCJ";
	station["潼南"]="TVW";
	station["洮南"]="TVT";
	station["太平川"]="TIT";
	station["太平镇"]="TEB";
	station["图强"]="TQX";
	station["台前"]="TTK";
	station["天桥岭"]="TQL";
	station["土桥子"]="TQJ";
	station["汤山城"]="TCT";
	station["桃山"]="TAB";
	station["塔石嘴"]="TIM";
	station["汤旺河"]="THB";
	station["同心"]="TXJ";
	station["土溪"]="TSW";
	station["桐乡"]="TCH";
	station["田阳"]="TRZ";
	station["桃映"]="TKQ";
	station["天义"]="TND";
	station["汤阴"]="TYF";
	station["驼腰岭"]="TIL";
	station["太阳山"]="TYJ";
	station["汤原"]="TYB";
	station["塔崖驿"]="TYP";
	station["滕州东"]="TEK";
	station["台州"]="TZH";
	station["天祝"]="TZJ";
	station["滕州"]="TXK";
	station["天镇"]="TZV";
	station["桐子林"]="TEW";
	station["天柱山"]="QWH";
	station["文安"]="WBP";
	station["武安"]="WAP";
	station["王安镇"]="WVP";
	station["五叉沟"]="WCT";
	station["文昌"]="WEQ";
	station["温春"]="WDB";
	station["五大连池"]="WRB";
	station["文登"]="WBK";
	station["五道沟"]="WDL";
	station["五道河"]="WHP";
	station["文地"]="WNZ";
	station["卫东"]="WVT";
	station["武当山"]="WRN";
	station["望都"]="WDP";
	station["乌尔旗汗"]="WHX";
	station["潍坊"]="WFK";
	station["万发屯"]="WFB";
	station["王府"]="WUT";
	station["瓦房店西"]="WXT";
	station["王岗"]="WGB";
	station["武功"]="WGY";
	station["湾沟"]="WGL";
	station["吴官田"]="WGM";
	station["乌海"]="WVC";
	station["苇河"]="WHB";
	station["卫辉"]="WHF";
	station["吴家川"]="WCJ";
	station["五家"]="WUB";
	station["威箐"]="WAM";
	station["午汲"]="WJP";
	station["王家湾"]="WJJ";
	station["倭肯"]="WQB";
	station["五棵树"]="WKT";
	station["五龙背"]="WBT";
	station["乌兰哈达"]="WLC";
	station["万乐"]="WEB";
	station["瓦拉干"]="WVX";
	station["温岭"]="VHH";
	station["五莲"]="WLK";
	station["乌拉特前旗"]="WQC";
	station["乌拉山"]="WSC";
	station["卧里屯"]="WLX";
	station["渭南北"]="WBY";
	station["乌奴耳"]="WRX";
	station["万宁"]="WNQ";
	station["万年"]="WWG";
	station["渭南南"]="WVY";
	station["渭南镇"]="WNJ";
	station["沃皮"]="WPT";
	station["吴堡"]="WUY";
	station["吴桥"]="WUP";
	station["汪清"]="WQL";
	station["武清"]="WWP";
	station["温泉"]="WQM";
	station["武山"]="WSJ";
	station["文水"]="WEV";
	station["魏善庄"]="WSP";
	station["王瞳"]="WTP";
	station["五台山"]="WSV";
	station["王团庄"]="WZJ";
	station["五五"]="WVR";
	station["无锡东"]="WGH";
	station["卫星"]="WVB";
	station["闻喜"]="WXV";
	station["武乡"]="WVV";
	station["无锡新区"]="IFH";
	station["武穴"]="WXN";
	station["吴圩"]="WYZ";
	station["王杨"]="WYB";
	station["五营"]="WWB";
	station["武义"]="RYH";
	station["瓦窑田"]="WIM";
	station["五原"]="WYC";
	station["苇子沟"]="WZL";
	station["韦庄"]="WZY";
	station["五寨"]="WZV";
	station["王兆屯"]="WZB";
	station["微子镇"]="WQP";
	station["魏杖子"]="WKD";
	station["新安"]="EAM";
	station["兴安"]="XAZ";
	station["新安县"]="XAF";
	station["新保安"]="XAP";
	station["下板城"]="EBP";
	station["西八里"]="XLP";
	station["宣城"]="ECH";
	station["兴城"]="XCD";
	station["小村"]="XEM";
	station["新绰源"]="XRX";
	station["下城子"]="XCB";
	station["喜德"]="EDW";
	station["小得江"]="EJM";
	station["西大庙"]="XMP";
	station["小董"]="XEZ";
	station["小东"]="XOD";
	station["西斗铺"]="XPC";
	station["息烽"]="XFW";
	station["信丰"]="EFG";
	station["襄汾"]="XFV";
	station["新干"]="EGG";
	station["孝感"]="XGN";
	station["西固城"]="XUJ";
	station["夏官营"]="XGJ";
	station["西岗子"]="NBB";
	station["襄河"]="XXB";
	station["新和"]="XIR";
	station["宣和"]="XWJ";
	station["斜河涧"]="EEP";
	station["新华屯"]="XAX";
	station["新华"]="XHB";
	station["新化"]="EHQ";
	station["宣化"]="XHP";
	station["兴和西"]="XEC";
	station["小河沿"]="XYD";
	station["下花园"]="XYP";
	station["小河镇"]="EKY";
	station["徐家"]="XJB";
	station["峡江"]="EJG";
	station["新绛"]="XJV";
	station["辛集"]="ENP";
	station["新江"]="XJM";
	station["西街口"]="EKM";
	station["许家屯"]="XJT";
	station["许家台"]="XTJ";
	station["谢家镇"]="XMT";
	station["兴凯"]="EKB";
	station["小榄"]="EAQ";
	station["香兰"]="XNB";
	station["兴隆店"]="XDD";
	station["新乐"]="ELP";
	station["新林"]="XPX";
	station["小岭"]="XLB";
	station["新李"]="XLJ";
	station["西林"]="XYB";
	station["西柳"]="GCT";
	station["仙林"]="XPH";
	station["新立屯"]="XLD";
	station["小路溪"]="XLM";
	station["兴隆镇"]="XZB";
	station["新立镇"]="XGT";
	station["新民"]="XMD";
	station["西麻山"]="XMB";
	station["下马塘"]="XAT";
	station["孝南"]="XNV";
	station["咸宁北"]="XRN";
	station["兴宁"]="ENQ";
	station["咸宁"]="XNN";
	station["西平"]="XPN";
	station["兴平"]="XPY";
	station["新坪田"]="XPM";
	station["霞浦"]="XOS";
	station["溆浦"]="EPQ";
	station["犀浦"]="XIW";
	station["新青"]="XQB";
	station["新邱"]="XQD";
	station["兴泉堡"]="XQJ";
	station["仙人桥"]="XRL";
	station["小寺沟"]="ESP";
	station["杏树"]="XSB";
	station["夏石"]="XIZ";
	station["浠水"]="XZN";
	station["下社"]="XSV";
	station["徐水"]="XSP";
	station["小哨"]="XAM";
	station["新松浦"]="XOB";
	station["杏树屯"]="XDT";
	station["许三湾"]="XSJ";
	station["湘潭"]="XTQ";
	station["邢台"]="XTP";
	station["仙桃西"]="XAN";
	station["下台子"]="EIP";
	station["徐闻"]="XJQ";
	station["新窝铺"]="EPD";
	station["修武"]="XWF";
	station["新县"]="XSN";
	station["息县"]="ENN";
	station["西乡"]="XQY";
	station["湘乡"]="XXQ";
	station["西峡"]="XIF";
	station["孝西"]="XOV";
	station["小新街"]="XXM";
	station["新兴县"]="XGQ";
	station["西小召"]="XZC";
	station["小西庄"]="XXP";
	station["向阳"]="XDB";
	station["旬阳"]="XUY";
	station["旬阳北"]="XBY";
	station["襄阳东"]="XWN";
	station["兴业"]="SNZ";
	station["小雨谷"]="XHM";
	station["信宜"]="EEQ";
	station["小月旧"]="XFM";
	station["小扬气"]="XYX";
	station["祥云"]="EXM";
	station["襄垣"]="EIF";
	station["夏邑县"]="EJH";
	station["新友谊"]="EYB";
	station["新阳镇"]="XZJ";
	station["徐州东"]="UUH";
	station["新帐房"]="XZX";
	station["悬钟"]="XRP";
	station["新肇"]="XZT";
	station["忻州"]="XXV";
	station["汐子"]="XZD";
	station["西哲里木"]="XRD";
	station["新杖子"]="ERP";
	station["姚安"]="YAC";
	station["依安"]="YAX";
	station["永安"]="YAS";
	station["永安乡"]="YNB";
	station["渔坝村"]="YBM";
	station["亚布力"]="YBB";
	station["元宝山"]="YUD";
	station["羊草"]="YAB";
	station["秧草地"]="YKM";
	station["阳澄湖"]="AIH";
	station["迎春"]="YYB";
	station["叶城"]="YER";
	station["盐池"]="YKJ";
	station["砚川"]="YYY";
	station["阳春"]="YQQ";
	station["宜城"]="YIN";
	station["应城"]="YHN";
	station["禹城"]="YCK";
	station["晏城"]="YEK";
	station["羊场"]="YED";
	station["阳城"]="YNF";
	station["阳岔"]="YAL";
	station["郓城"]="YPK";
	station["雁翅"]="YAP";
	station["云彩岭"]="ACP";
	station["虞城县"]="IXH";
	station["营城子"]="YCT";
	station["永登"]="YDJ";
	station["英德"]="YDQ";
	station["尹地"]="YDM";
	station["永定"]="YGS";
	station["雁荡山"]="YGH";
	station["于都"]="YDG";
	station["园墩"]="YAJ";
	station["英德西"]="IIQ";
	station["永福"]="YFZ";
	station["永丰营"]="YYM";
	station["杨岗"]="YRB";
	station["阳高"]="YOV";
	station["阳谷"]="YIK";
	station["友好"]="YOB";
	station["余杭"]="EVH";
	station["沿河城"]="YHP";
	station["岩会"]="AEP";
	station["羊臼河"]="YHM";
	station["永嘉"]="URH";
	station["营街"]="YAM";
	station["盐津"]="AEW";
	station["余江"]="YHG";
	station["叶集"]="YCH";
	station["燕郊"]="AJP";
	station["姚家"]="YAT";
	station["岳家井"]="YGJ";
	station["一间堡"]="YJT";
	station["英吉沙"]="YIR";
	station["云居寺"]="AFP";
	station["燕家庄"]="AZK";
	station["永康"]="RFH";
	station["营口东"]="YGT";
	station["银浪"]="YJX";
	station["永郎"]="YLW";
	station["宜良北"]="YSM";
	station["永乐店"]="YDY";
	station["伊拉哈"]="YLX";
	station["伊林"]="YLB";
	station["彝良"]="ALW";
	station["杨林"]="YLM";
	station["余粮堡"]="YLD";
	station["杨柳青"]="YQP";
	station["月亮田"]="YUM";
	station["亚龙湾"]="TWQ";
	station["杨陵镇"]="YSY";
	station["义马"]="YMF";
	station["云梦"]="YMN";
	station["元谋"]="YMM";
	station["一面山"]="YST";
	station["玉门镇"]="YXJ";
	station["沂南"]="YNK";
	station["宜耐"]="YVM";
	station["伊宁东"]="YNR";
	station["一平浪"]="YIM";
	station["营盘水"]="YZJ";
	station["羊堡"]="ABM";
	station["营盘湾"]="YPC";
	station["阳泉北"]="YPP";
	station["乐清"]="UPH";
	station["焉耆"]="YSR";
	station["源迁"]="AQK";
	station["姚千户屯"]="YQT";
	station["阳曲"]="YQV";
	station["榆树沟"]="YGP";
	station["月山"]="YBF";
	station["玉石"]="YSJ";
	station["偃师"]="YSF";
	station["沂水"]="YUK";
	station["榆社"]="YSV";
	station["颍上"]="YVH";
	station["窑上"]="ASP";
	station["元氏"]="YSP";
	station["杨树岭"]="YAD";
	station["野三坡"]="AIP";
	station["榆树屯"]="YSX";
	station["榆树台"]="YUT";
	station["鹰手营子"]="YIP";
	station["源潭"]="YTQ";
	station["牙屯堡"]="YTZ";
	station["烟筒山"]="YSL";
	station["烟筒屯"]="YUX";
	station["羊尾哨"]="YWM";
	station["越西"]="YHW";
	station["攸县"]="YOG";
	station["永修"]="ACG";
	station["酉阳"]="AFW";
	station["余姚"]="YYH";
	station["弋阳东"]="YIG";
	station["岳阳东"]="YIQ";
	station["阳邑"]="ARP";
	station["鸭园"]="YYL";
	station["鸳鸯镇"]="YYJ";
	station["燕子砭"]="YZY";
	station["宜州"]="YSZ";
	station["仪征"]="UZH";
	station["兖州"]="YZK";
	station["迤资"]="YQM";
	station["羊者窝"]="AEM";
	station["杨杖子"]="YZD";
	station["镇安"]="ZEY";
	station["治安"]="ZAD";
	station["招柏"]="ZBP";
	station["张百湾"]="ZUP";
	station["枝城"]="ZCN";
	station["子长"]="ZHY";
	station["诸城"]="ZQK";
	station["邹城"]="ZIK";
	station["赵城"]="ZCV";
	station["章党"]="ZHT";
	station["肇东"]="ZDB";
	station["照福铺"]="ZFM";
	station["章古台"]="ZGD";
	station["赵光"]="ZGB";
	station["中和"]="ZHX";
	station["中华门"]="VNH";
	station["枝江北"]="ZIN";
	station["钟家村"]="ZJY";
	station["朱家沟"]="ZUB";
	station["紫荆关"]="ZYP";
	station["周家"]="ZOB";
	station["诸暨"]="ZDH";
	station["镇江南"]="ZEH";
	station["周家屯"]="ZOD";
	station["郑家屯"]="ZJD";
	station["褚家湾"]="CWJ";
	station["湛江西"]="ZWQ";
	station["朱家窑"]="ZUJ";
	station["曾家坪子"]="ZBW";
	station["镇赉"]="ZLT";
	station["枣林"]="ZIV";
	station["扎鲁特"]="ZLD";
	station["扎赉诺尔西"]="ZXX";
	station["樟木头"]="ZOQ";
	station["中牟"]="ZGF";
	station["中宁东"]="ZDJ";
	station["中宁"]="VNJ";
	station["中宁南"]="ZNJ";
	station["镇平"]="ZPF";
	station["漳平"]="ZPS";
	station["泽普"]="ZPR";
	station["枣强"]="ZVP";
	station["张桥"]="ZQY";
	station["章丘"]="ZTK";
	station["朱日和"]="ZRC";
	station["泽润里"]="ZLM";
	station["中山北"]="ZGQ";
	station["樟树东"]="ZOG";
	station["中山"]="ZSQ";
	station["柞水"]="ZSY";
	station["钟山"]="ZSZ";
	station["樟树"]="ZSG";
	station["张台子"]="ZZT";
	station["珠窝"]="ZOP";
	station["张维屯"]="ZWB";
	station["彰武"]="ZWD";
	station["棕溪"]="ZOY";
	station["钟祥"]="ZTN";
	station["资溪"]="ZXS";
	station["镇西"]="ZVT";
	station["张辛"]="ZIP";
	station["正镶白旗"]="ZXC";
	station["紫阳"]="ZVY";
	station["枣阳"]="ZYN";
	station["竹园坝"]="ZAW";
	station["张掖"]="ZYJ";
	station["镇远"]="ZUW";
	station["朱杨溪"]="ZXW";
	station["漳州东"]="GOS";
	station["漳州"]="ZUS";
	station["壮志"]="ZUX";
	station["子洲"]="ZZY";
	station["中寨"]="ZZM";
	station["涿州"]="ZXP";
	station["咋子"]="ZAL";
	station["卓资山"]="ZZC";
	station["株洲西"]="ZAQ";
	station["安阳东"]="ADF";
	station["保定东"]="BMP";
	station["东二道河"]="DRB";
	station["大苴"]="DIM";
	station["大青沟"]="DSD";
	station["定州东"]="DOP";
	station["富川"]="FDZ";
	station["抚远"]="FYB";
	station["高碑店东"]="GMP";
	station["革居"]="GEM";
	station["光明城"]="IMQ";
	station["高邑西"]="GNP";
	station["鹤壁东"]="HFF";
	station["寒葱沟"]="HKB";
	station["邯郸东"]="HPP";
	station["合肥北城"]="COH";
	station["洪河"]="HPB";
	station["虎门"]="IUQ";
	station["哈密南"]="HLR";
	station["淮南东"]="HOH";
	station["库伦"]="KLD";
	station["漯河西"]="LBN";
	station["明港东"]="MDN";
	station["前锋"]="QFB";
	station["庆盛"]="QSQ";
	station["深圳北"]="IOQ";
	station["许昌东"]="XVF";
	station["孝感北"]="XJN";
	station["邢台东"]="EDP";
	station["新乡东"]="EGF";
	station["西阳村"]="XQF";
	station["信阳东"]="OYN";
	station["正定机场"]="ZHP";
	station["驻马店西"]="ZLN";
	station["卓资东"]="ZDC";
	station["涿州东"]="ZAP";
	station["郑州东"]="ZAF";
}


void CHuoche::LoadPassanger(void)
{
    CString fullname,idcard,phone;
	CString fullname2,idcard2,phone2;
	CString fullname3,idcard3,phone3;
	this->dlg->GetDlgItem(IDC_FULLNAME)->GetWindowText(fullname);
	this->dlg->GetDlgItem(IDC_IDCARD)->GetWindowText(idcard);
	this->dlg->GetDlgItem(IDC_PHONE)->GetWindowText(phone);

	this->dlg->GetDlgItem(IDC_FULLNAME2)->GetWindowText(fullname2);
	this->dlg->GetDlgItem(IDC_IDCARD2)->GetWindowText(idcard2);
	this->dlg->GetDlgItem(IDC_PHONE2)->GetWindowText(phone2);

	this->dlg->GetDlgItem(IDC_FULLNAME3)->GetWindowText(fullname3);
	this->dlg->GetDlgItem(IDC_IDCARD3)->GetWindowText(idcard3);
	this->dlg->GetDlgItem(IDC_PHONE3)->GetWindowText(phone3);

    this->passanger_name=fullname.GetBuffer();
    this->passanger_idcard=idcard.GetBuffer();
    this->passanger_phone=phone.GetBuffer();

    this->passanger_name1=fullname2.GetBuffer();
    this->passanger_idcard1=idcard2.GetBuffer();
	this->passanger_phone1=phone2.GetBuffer();

	this->passanger_name2=fullname3.GetBuffer();
    this->passanger_idcard2=idcard3.GetBuffer();
	this->passanger_phone2=phone3.GetBuffer();

    passanger_name=echttp::UrlEncode(echttp::Utf8Encode(passanger_name));
	passanger_name1=echttp::UrlEncode(echttp::Utf8Encode(passanger_name1));
	passanger_name2=echttp::UrlEncode(echttp::Utf8Encode(passanger_name2));

}


void CHuoche::LoadDomain(void)
{
    CString domain;
	this->dlg->GetDlgItem(IDC_EDIT_DOMAIN)->GetWindowText(domain);
    this->domain=domain.GetBuffer();
    if(this->domain=="") this->domain="kyfw.12306.cn";
}


void CHuoche::CheckUserOnline(void)
{
    this->http->Post("https://"+this->domain+"/otn/login/checkUser","_json_att=",boost::bind(&CHuoche::RecvCheckUserOnline,this,_1));
}

void CHuoche::RecvCheckUserOnline(boost::shared_ptr<echttp::respone> respone)
{
    std::string result;
	if (respone->as_string()!=""){
		result=respone->as_string();

        if(result.find("data\":{\"flag\":false")!=std::string::npos)
        {
            this->showMsg("已掉线，请重新登录！！！");
        }else{
            this->showMsg("用户在线状态正常");
        }

    }else{
        this->showMsg("检查用户在线状态返回空白!!!");
    }
}

void CHuoche::RecvNothing(boost::shared_ptr<echttp::respone> respone)
{
    return;
}


bool CHuoche::CheckQueueCount(Ticket ticket, std::string token)
{
    std::string pstr="train_date="+echttp::UrlEncode(echttp::Date2UTC(ticket.start_train_date))+"&train_no="+ticket.train_no
                +"&stationTrainCode="+ticket.station_train_code+"&seatType="+ticket.seat_type+"&fromStationTelecode="
                +ticket.from_station_telecode+"&toStationTelecode="+ticket.to_station_telecode+"&leftTicket="+ticket.yp_info
                +"&purpose_codes=00&_json_att=&REPEAT_SUBMIT_TOKEN=5fed7925e6b4cce795f2091eaa041a90"+token;

	std::string url="https://kyfw.12306.cn/otn/confirmPassenger/getQueueCount";
	std::string queue_result=this->http->Post(url,pstr)->as_string();

    if(queue_result.find("op_2\":\"false")!=std::string::npos)
    {
        return true;
    }else{
        return false;
    }

}
