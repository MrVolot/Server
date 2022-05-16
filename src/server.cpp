#include "server.h"
//#include <mutex>
//#include <chrono>
//
//std::mutex mtx;
//
//using namespace std::chrono_literals;
Server::Server(io_service& service) : service_{ service }, acceptor_{ service, ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 10678) }
{
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
    //verificateHash() TODO
    connection->getStrBuf().reset(new boost::asio::streambuf);
    connection->setMutableBuffer();
    connection->callRead();
}

void Server::writeCallback(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred)
{

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
//
//void Server::checker()
//{
//    while (true) {
//        mtx.lock();
//        for (auto& n : connections) {
//            std::cout << std::boolalpha <<n->getSocket().is_open() << " id: " << n->getId() << "\n";
//        }
//        mtx.unlock();
//        std::this_thread::sleep_for(5s);
//    }
//}
