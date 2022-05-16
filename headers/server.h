#pragma once
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "JsonStructures.h"
#include "Database.h"
#include "ConnectionHandler.h"

using namespace boost::asio;

class Server
{
    boost::asio::io_service& service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    inline static std::vector<std::shared_ptr<IConnectionHandler<Server>>> connections;
public:
    Server(boost::asio::io_service &service);
    void handleAccept(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err);
    void startAccept();
    void readConnection(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred);
    void writeCallback(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred);
    static void writer(std::string str, unsigned long long idTo, unsigned long long idFrom);
    static void sendOnlineResponse(unsigned long long id);
    void checker();
};

