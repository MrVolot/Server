#pragma once
#include <boost/asio.hpp>
#include "ConnectionHandler.h"

class Client {
	std::string name_;
	unsigned long long id_;
	std::string publicKey_;
public:
	Client(const std::string& name, unsigned long long id, const std::string& publicKey);
	unsigned long long getId() const;
	std::string& getName();
	std::string& getPublicKey();
	bool operator==(const Client& rhs) const;
	
	bool onlineStatus;
};