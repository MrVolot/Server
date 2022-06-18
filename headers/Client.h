#pragma once
#include <boost/asio.hpp>
#include "ConnectionHandler.h"

class Client {
	std::string name_;
	unsigned long long id_;
public:
	Client(std::string name, unsigned long long id);
	unsigned long long getId() const;
	std::string& getName();
	bool operator==(const Client& rhs) const;
	
	bool onlineStatus;
};