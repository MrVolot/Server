#pragma once
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "connection_handler.h"
#include "JsonStructures.h"

using namespace boost::asio;

class Server
{
    boost::asio::io_service& service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    inline static std::vector<std::shared_ptr<Connection_Handler>> connections;
    unsigned long long idS;
public:
    Server(boost::asio::io_service &service);
    void handleAccept(std::shared_ptr<Connection_Handler> connection, const boost::system::error_code& err);
    void startAccept();
    static void writer(std::string str, unsigned long long idTo, unsigned long long idFrom);
    static void sendOnlineResponse(unsigned long long id);
    void checker();
};

