#pragma once
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <optional>
#include <unordered_map>
#include "Database.h"
//#include "ConnectionHandler.h"
#include "ConnectionHandlerSsl.h"
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
    boost::asio::ssl::context ssl_context_;

    Json::Value getJsonFriendList(const std::string& id);
    void sendFriendList(std::shared_ptr<IConnectionHandler<Server>> connection, const std::string& userId);
    void closeClientConnection(std::shared_ptr<IConnectionHandler<Server>> connection);
    void saveMessageToDatabase(const std::string& sender, const std::string& receiver, const std::string& msg);
    Json::Value getChatMessages(const std::string& chatName);
    void sendChatHistory(const std::string& id, Json::Value& chatHistory);
    void createChatTable(const std::string& tableName);
    std::string generateTableName(const std::string& sender, const std::string& receiver);
    Json::Value getLastMessage(const std::string& sender, const std::string& receiver);
    std::optional<Json::Value> tryGetContactInfo(const std::string& login);
    void sendPossibleContactsInfo(std::shared_ptr<IConnectionHandler<Server>> connection, const Json::Value& value);
    void insertFriendIfNeeded(const std::string& tableName, std::pair<const std::string&, const std::string&> value);
    void verifyFriendsConnection(const std::string& sender, const std::string& receiver);
    std::string getPublicKey(const std::string& id);

    bool custom_verify_callback(bool preverified, boost::asio::ssl::verify_context& ctx) {
        // You can implement your custom verification logic here.
        // For now, we'll just return the value of 'preverified'.
            // Get the X509_STORE_CTX object
        X509_STORE_CTX* store_ctx = ctx.native_handle();

        // Get the current certificate and its depth in the chain
        int depth = X509_STORE_CTX_get_error_depth(store_ctx);
        X509* cert = X509_STORE_CTX_get_current_cert(store_ctx);

        // Convert the X509 certificate to a human-readable format
        BIO* bio = BIO_new(BIO_s_mem());
        X509_print(bio, cert);
        BUF_MEM* mem;
        BIO_get_mem_ptr(bio, &mem);
        std::string cert_info(mem->data, mem->length);
        BIO_free(bio);

        std::cout << "Certificate depth: " << depth << std::endl;
        std::cout << "Certificate information: " << std::endl << cert_info << std::endl;
        std::cout << "Preverified: " << preverified << std::endl;
        return true;
    }
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

