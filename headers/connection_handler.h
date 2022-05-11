#pragma once
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "json/json.h"
#include <iostream>
//#include "JsonStructures.h"  ??
#include "Client.h"

class Connection_Handler: public std::enable_shared_from_this<Connection_Handler>
{
    boost::asio::ip::tcp::socket socket_;
    boost::asio::io_service& service_;
    const size_t msgLength{1024};
    std::unique_ptr<boost::asio::streambuf> strBuf;
    boost::asio::streambuf::mutable_buffers_type mutableBuffer;
    char buffer[1024];
    unsigned long long id{0};
    std::unique_ptr<Client> clientInfo;
public:
    Connection_Handler(boost::asio::io_service& service, unsigned long long id_);
    ~Connection_Handler();
    boost::asio::ip::tcp::socket& getSocket();
    void start();
    void readHandle(const boost::system::error_code& err, size_t bytes_transferred);
    void writeHandle(const boost::system::error_code& err, size_t bytes_transferred);
    static std::shared_ptr<Connection_Handler> create(boost::asio::io_service& service, unsigned long long id_);
    unsigned long long getId();
    void writeMessage(const std::string& str);
    bool isAvailable();
    void sendTokenResponse(bool status);
};
