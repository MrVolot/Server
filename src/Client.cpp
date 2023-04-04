#include "Client.h"
#include <iostream>
Client::Client(const std::string& name, unsigned long long id, const std::string& publicKey) :
	name_{ name },
	id_{ id },
	onlineStatus{false},
	publicKey_{ publicKey }
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

std::string& Client::getPublicKey()
{
	return publicKey_;
}

bool Client::operator==(const Client& rhs) const
{
	return rhs.name_ == name_;
}
