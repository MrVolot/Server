#pragma once
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <optional>
#include <unordered_map>
#include "Database.h"
#include "ConnectionHandler.h"
#include "Client.h"
#include "json/json.h"
#include <mutex>

using namespace boost::asio;


class Server
{
    boost::asio::io_service& service_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<std::string, std::pair<std::unique_ptr<Client>, std::shared_ptr<IConnectionHandler<Server>>>> connections_;
    DatabaseHandler& databaseInstance;
    std::mutex mutex;

    std::string getJsonFriendList(const std::string& id);
    void sendFriendList(std::shared_ptr<IConnectionHandler<Server>> connection, const std::string& userId);
    void closeClientConnection(std::shared_ptr<IConnectionHandler<Server>> connection);
    void saveMessageToDatabase(const std::string& sender, const std::string& receiver, const std::string& msg);
    Json::Value getChatMessages(const std::string& chatName);
    void sendChatHistory(const std::string& id, Json::Value& chatHistory);
    void createChatTable(const std::string& tableName);
    std::string generateTableName(const std::string& sender, const std::string& receiver);
    std::string getLastMessage(const std::string& sender, const std::string& receiver);
public:
    Server(boost::asio::io_service &service);
    void handleAccept(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err);
    void startAccept();
    void readConnection(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred);
    void writeCallback(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred);
    std::optional<std::vector<std::vector<std::string>>> verificateHash(const std::string& hash);
    void callbackReadCommand(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred);
    void sendMessageToClient(const std::string& to, const std::string& what, const std::string& from);
    void loadUsers();
    Client& findClientByConnection(std::shared_ptr<IConnectionHandler<Server>> connection);
    void pingClient();
};

