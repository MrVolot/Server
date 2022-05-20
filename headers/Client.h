#pragma once
#include <boost/asio.hpp>
#include "ConnectionHandler.h"

class Client {
	std::string name_;
	unsigned long long id_;
	std::shared_ptr<IConnectionHandler<Client>> connection_{nullptr};
public:
	Client(std::string name, unsigned long long id, boost::asio::io_service& service);
	unsigned long long getId();
	std::string& getName();
	std::shared_ptr<IConnectionHandler<Client>> getConnection();
};