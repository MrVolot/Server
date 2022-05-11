#include "Client.h"

Client::Client(std::string name_, unsigned long long id_) : name{name_}, id{ id_ } {}

unsigned long long Client::getId()
{
	return id;
}

std::string& Client::getName()
{
	return name;
}
