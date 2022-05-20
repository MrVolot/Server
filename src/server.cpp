#include "server.h"
#include "json/json.h"
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
    connections.push_back(std::make_shared<ConnectionHandler<Server>>(service_, *this));
    connections.back()->setReadCallback(&Server::readConnection);
    connections.back()->setWriteCallback(&Server::writeCallback);
    acceptor_.async_accept(connections.back()->getSocket(), boost::bind(&Server::handleAccept, this, connections.back(),boost::asio::placeholders::error));
}

void Server::readConnection(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred)
{
    if (err) {
        connection->getSocket().close();
        return;
    }
    std::string data{ boost::asio::buffer_cast<const char*>(connection->getStrBuf()->data()) };
    if (auto result{ verificateHash(data) }; result != std::nullopt) {
        Client tempClient( result.value()[0][1], std::stoll(result.value()[0][0]), std::move(connection->getIoService()) );
        clients_.insert({ tempClient.getId(), tempClient });
        //tempClient.getConnection()->setReadCallback(&Server::readHandleTest);
        try {
            connection.reset();
            tempClient.getConnection()->getStrBuf().reset(new boost::asio::streambuf);
            tempClient.getConnection()->setMutableBuffer();
            tempClient.getConnection()->callRead();
            //tempClient.getConnection()->callWrite("kuku");
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