#include "server.h"
#include <iostream>

Server::Server(io_service& service) : service_{ service }, acceptor_{ service, ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 10678) },
databaseInstance{ DatabaseHandler::getInstance() }
{
    databaseInstance.connectDB("Server", "123");
    startAccept();
}

void Server::handleAccept(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code &err)
{
    if(!err){
        connection->callRead();
    }
    startAccept();
}

void Server::startAccept()
{
    connections_.insert({ "test", std::make_pair(nullptr, std::make_shared<ConnectionHandler<Server>>(service_, *this))});
    connections_.at("test").second->setReadCallback(&Server::readConnection);
    connections_.at("test").second->setWriteCallback(&Server::writeCallback);
    acceptor_.async_accept(connections_.at("test").second->getSocket(), boost::bind(&Server::handleAccept, this, connections_.at("test").second,boost::asio::placeholders::error));
    connections_.erase("test");
}

void Server::readConnection(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred)
{
    if (err) {
        connection->getSocket().close();
        return;
    }
    std::string data{ boost::asio::buffer_cast<const char*>(connection->getStrBuf()->data()) };
    if (auto result{ verificateHash(data) }; result != std::nullopt) {
        std::unique_ptr<Client> tempClient{ new Client {result.value()[0][1], std::stoull(result.value()[0][0])} };
        std::string clientName{ tempClient->getName() };
        connections_.insert({ tempClient->getName(), std::make_pair(std::move(tempClient),connection) });
        connections_.at(clientName).second->setReadCallback(&Server::readHandleTest);
        try {
            //connection.reset();
            connections_.at(clientName).second->getStrBuf().reset(new boost::asio::streambuf);
            connections_.at(clientName).second->setMutableBuffer();
            connections_.at(clientName).second->callRead();
            connections_.at(clientName).second->callWrite("kuku");
        }
        catch (std::exception& ex) {
            std::cout << ex.what();
        }
    }
}

void Server::writeCallback(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred)
{

}

std::optional<std::vector<std::vector<std::string>>> Server::verificateHash(const std::string& hash)
{
    auto result{ databaseInstance.executeQuery("SELECT ID, LOGIN FROM CONTACTS WHERE TOKEN = '" + hash + "'")};
    if (result.empty()) {
        return std::nullopt;
    }
    return result;
}

void Server::readHandleTest(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred)
{
    std::cout << "test call back\n";
}

//void Server::writer(std::string str, unsigned long long idTo, unsigned long long idFrom)
//{
//    auto clientFrom{ std::find_if(connections.begin(), connections.end(), [idFrom](const std::shared_ptr< ConnectionHandler<Server>>& id_) {return id_->getId() == idFrom; }) };
//    auto clientTo{ std::find_if(connections.begin(), connections.end(), [idTo](const std::shared_ptr< ConnectionHandler<Server>>& id_) {return id_->getId() == idTo; }) };
//    if (clientTo == connections.end() || !clientTo->get()->isAvailable()) {
//        return;
//    }
//    if (clientTo->get()->getId() == clientFrom->get()->getId()) {
//        return;
//    }
//    boost::system::error_code erc;
//    clientTo->get()->writeMessage(str);
//}
//
//void Server::sendOnlineResponse(unsigned long long id)
//{
//    auto clientTo{ std::find_if(connections.begin(), connections.end(), [id](const std::shared_ptr< ConnectionHandler<Server>>& id_) {return id_->getId() == id; }) };
//    if (clientTo != connections.end()) {
//        Json::Value tmpValue;
//        Json::FastWriter writer;
//        tmpValue["online"] = "true";
//        clientTo->get()->writeMessage(writer.write(tmpValue));
//    }
//}