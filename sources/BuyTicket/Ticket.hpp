#pragma once
#include <string>
#include "echttp/function.hpp"

class Ticket
{
public:
    //座位信息
    std::string first_seat;
    std::string second_seat;
    std::string soft_bed;
    std::string hard_bed;
    std::string hard_seat;

    //车站信息
    std::string train_no;
    std::string station_train_code;//车次编号，例如K540
    std::string from_station_telecode;
    std::string from_station_name;
    std::string to_station_telecode;
    std::string to_station_name;
    std::string yp_info;//未知信息
    std::string location_code;
    std::string secret_str;
    std::string start_train_date;//乘车日期，例如20140127

    //乘车信息
    std::string train_date;
    std::string seat_type;

    Ticket(std::string ticket_str)
    {
        this->train_no=echttp::substr(ticket_str,"train_no\":\"","\"");
        this->from_station_telecode=echttp::substr(ticket_str,"from_station_telecode\":\"","\"");
        this->from_station_name=echttp::substr(ticket_str,"from_station_name\":\"","\"");
        this->to_station_telecode=echttp::substr(ticket_str,"to_station_telecode\":\"","\"");
        this->to_station_name=echttp::substr(ticket_str,"to_station_name\":\"","\"");
        this->yp_info=echttp::substr(ticket_str,"yp_info\":\"","\"");
        this->start_train_date=echttp::substr(ticket_str,"start_train_date\":\"","\"");
        this->train_date=echttp::DateFormat(this->start_train_date,"%Y-%m-%d");
        this->location_code=echttp::substr(ticket_str,"location_code\":\"","\"");
        this->secret_str=echttp::substr(ticket_str,"secretStr\":\"","\"");
        this->station_train_code=echttp::substr(ticket_str,"station_train_code\":\"","\"");

        this->first_seat=echttp::substr(ticket_str,"\"zy_num\":\"","\"");
        this->second_seat=echttp::substr(ticket_str,"\"ze_num\":\"","\"");
        this->soft_bed=echttp::substr(ticket_str,"\"rw_num\":\"","\"");
        this->hard_bed=echttp::substr(ticket_str,"\"yw_num\":\"","\"");
        this->hard_seat=echttp::substr(ticket_str,"\"yz_num\":\"","\"");
    }

    void SetBuySeat(std::string seat)
    {
        this->seat_type=seat;
    }
};

