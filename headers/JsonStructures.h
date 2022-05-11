#pragma once
#include <string>
#include <boost/asio.hpp>

struct Information
{
    std::string from;
    std::string to;
    std::string message;
};

struct Client_Info {
    Information info;
    boost::asio::ip::tcp::socket socket;
    unsigned long long id;
};