#pragma once
#include <boost/asio.hpp>

class Client {
	std::string name;
	unsigned long long id;
public:
	Client(std::string name_, unsigned long long id_);
	unsigned long long getId();
	std::string& getName();
};