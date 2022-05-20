#include "Client.h"

Client::Client(std::string name, unsigned long long id, boost::asio::io_service& service) :
	name_{ name }, id_{ id }
{
	connection_.reset(new ConnectionHandler<Client>{ service, *this });
}

unsigned long long Client::getId()
{
	return id_;
}

std::string& Client::getName()
{
	return name_;
}

std::shared_ptr<IConnectionHandler<Client>> Client::getConnection()
{
	return connection_;
}