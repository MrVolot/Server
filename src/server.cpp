#include "server.h"
#include "Database.h"
#include <mutex>
#include <chrono>

std::mutex mtx;

using namespace std::chrono_literals;
Server::Server(io_service& service) : service_{ service }, acceptor_{ service, ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 10678) }, idS{ 0 }
{
    startAccept();
}

void Server::handleAccept(std::shared_ptr<Connection_Handler> connection, const boost::system::error_code &err)
{
    if(!err){
        connection->start();
        std::thread trd{ &Server::checker, this };
        trd.detach();
    }
    startAccept();
}

void Server::startAccept()
{
    connections.push_back(Connection_Handler::create(service_, ++idS));
    acceptor_.async_accept(connections.back()->getSocket(), boost::bind(&Server::handleAccept, this, connections.back(),boost::asio::placeholders::error));
}

void Server::writer(std::string str, unsigned long long idTo, unsigned long long idFrom)
{
    auto clientFrom{ std::find_if(connections.begin(), connections.end(), [idFrom](const std::shared_ptr<Connection_Handler>& id_) {return id_->getId() == idFrom; }) };
    auto clientTo{ std::find_if(connections.begin(), connections.end(), [idTo](const std::shared_ptr<Connection_Handler>& id_) {return id_->getId() == idTo; }) };
    if (clientTo == connections.end() || !clientTo->get()->isAvailable()) {
        return;
    }
    if (clientTo->get()->getId() == clientFrom->get()->getId()) {
        return;
    }
    boost::system::error_code erc;
    clientTo->get()->writeMessage(str);
}

void Server::sendOnlineResponse(unsigned long long id)
{
    auto clientTo{ std::find_if(connections.begin(), connections.end(), [id](const std::shared_ptr<Connection_Handler>& id_) {return id_->getId() == id; }) };
    if (clientTo != connections.end()) {
        Json::Value tmpValue;
        Json::FastWriter writer;
        tmpValue["online"] = "true";
        clientTo->get()->writeMessage(writer.write(tmpValue));
    }
}

void Server::checker()
{
    while (true) {
        mtx.lock();
        for (auto& n : connections) {
            std::cout << std::boolalpha <<n->getSocket().is_open() << " id: " << n->getId() << "\n";
        }
        mtx.unlock();
        std::this_thread::sleep_for(5s);
    }
}
