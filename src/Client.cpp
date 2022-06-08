#include "Client.h"
#include <iostream>
Client::Client(std::string name, unsigned long long id) :
	name_{ name }, id_{ id }
{

}

unsigned long long Client::getId() const
{
	return id_;
}

std::string& Client::getName()
{
	return name_;
}

bool Client::operator==(const Client& rhs) const
{
	return rhs.name_ == name_;
}
