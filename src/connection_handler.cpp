#include "connection_handler.h"
#include "server.h"
#include "Parser.h"
#include "Database.h"
#include <functional>
#include <iostream>
#include <memory>
#include <iterator>

using namespace boost::asio;

Connection_Handler::Connection_Handler(boost::asio::io_service& service, unsigned long long id_) : socket_{ service },
service_{ service }, strBuf{ new boost::asio::streambuf }, mutableBuffer{ strBuf->prepare(msgLength) }, id{id_} {
    memset(buffer, 0, msgLength);
    Database::getInstance().connectDB();
}

Connection_Handler::~Connection_Handler()
{
    Database::getInstance().disconnectDB();
}

boost::asio::ip::tcp::socket &Connection_Handler::getSocket()
{
    return socket_;
}

void Connection_Handler::start()
{
    socket_.async_read_some(boost::asio::buffer(mutableBuffer), boost::bind(&Connection_Handler::readHandle,
                                                                        shared_from_this(),
                                                                        boost::asio::placeholders::error,
                                                                        boost::asio::placeholders::bytes_transferred));

    
}

void Connection_Handler::readHandle(const boost::system::error_code &err, size_t bytes_transferred)
{
	if (err) {
		std::cout << "Error: " << err.message() << std::endl;
		socket_.close();
		return;
	}
	std::string data{ boost::asio::buffer_cast<const char*>(strBuf->data()) };
	auto result{ Parser::getInstance().checkIncomingData(data) };
	switch (result) {
	case Commands::CHECK_ONLINE: {

	}

	case Commands::SEND_MESSAGE: {

	}

	case Commands::TOKEN_VERIFICATION: {
		auto responseStatus{ Parser::getInstance().verifyToken(data) };
		sendTokenResponse(responseStatus);
	}
	}
    strBuf.reset(new boost::asio::streambuf);
    mutableBuffer = strBuf->prepare(msgLength);
    socket_.async_read_some(boost::asio::buffer(mutableBuffer), boost::bind(&Connection_Handler::readHandle,
        shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

void Connection_Handler::writeHandle(const boost::system::error_code &err, size_t bytes_transferred)
{
    if(err){
        std::cout<<"Error: "<<err.message()<<std::endl;
        socket_.close();
        return;
    }
    std::cout<<"Message was sent\n";
}

std::shared_ptr<Connection_Handler> Connection_Handler::create(boost::asio::io_service& service, unsigned long long id_)
{
    return std::make_shared<Connection_Handler>(service, id_);
}

unsigned long long Connection_Handler::getId()
{
    return id;
}

void Connection_Handler::writeMessage(const std::string& str)
{
    socket_.async_write_some(boost::asio::buffer(str.c_str(), str.size()), boost::bind(&Connection_Handler::writeHandle,
        shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

bool Connection_Handler::isAvailable()
{
    return clientInfo != nullptr;
}

void Connection_Handler::sendTokenResponse(bool status)
{
    Json::Value value;
    Json::FastWriter writer;
    value["command"] = "tokenVerification";
    if (status) {
        value["status"] = "true";
        value["token"] = Parser::getInstance().hash;
        writeMessage(writer.write(value));
        return;
    }
    value["status"] = "false";
    writeMessage(writer.write(value));
}
