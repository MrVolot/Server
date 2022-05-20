#pragma once
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <optional>
#include <unordered_map>
#include "Database.h"
#include "ConnectionHandler.h"
#include "Client.h"

using namespace boost::asio;


class Server
{
    boost::asio::io_service& service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    inline static std::vector<std::shared_ptr<IConnectionHandler<Server>>> connections;
    DatabaseHandler& databaseInstance;
    std::unordered_map<long long, Client> clients_;
public:
    Server(boost::asio::io_service &service);
    void handleAccept(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err);
    void startAccept();
    void readConnection(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred);
    void writeCallback(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred);
    std::optional<std::vector<std::vector<std::string>>> verificateHash(const std::string& hash);
    void readHandleTest(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred);
    static void writer(std::string str, unsigned long long idTo, unsigned long long idFrom);
    static void sendOnlineResponse(unsigned long long id);
};

