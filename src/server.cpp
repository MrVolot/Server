#include "server.h"
#include "Constants.h"
#include <iostream>

Server::Server(io_service& service) : service_{ service }, acceptor_{ service, ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 10690) },
databaseInstance{ DatabaseHandler::getInstance() }
{
	databaseInstance.connectDB("Server", "123");
	loadUsers();
	startAccept();
}

void Server::handleAccept(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err)
{
	if (!err) {
		connection->setAsyncReadCallback(&Server::readConnection);
		connection->setWriteCallback(&Server::writeCallback);
		connection->callAsyncRead();
	}
	startAccept();
}

void Server::startAccept()
{
	auto connection{ std::make_shared<ConnectionHandler<Server>>(service_, *this) };
	acceptor_.async_accept(connection->getSocket(), boost::bind(&Server::handleAccept, this, connection, boost::asio::placeholders::error));
}

void Server::readConnection(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred)
{
	if (err) {
		connection->getSocket().close();
		return;
	}
	std::lock_guard<std::mutex> guard(mutex);
	if (auto result{ verificateHash(connection->getData()) }; result != std::nullopt) {
		std::string clientName{ result.value()[0][0] };
		if (auto user{ connections_.find(clientName) }; user != connections_.end()) {
			user->second.second = std::move(connection);
			user->second.first->onlineStatus = true;
		}
		else {
			std::unique_ptr<Client> tempClient{ new Client {clientName, std::stoull(result.value()[0][0])} };
			tempClient->onlineStatus = true;
			connections_.insert({ tempClient->getName(), std::make_pair(std::move(tempClient),connection) });
		}
		auto clientId = connections_.at(clientName).first->getId();
		connections_.at(clientName).second->setAsyncReadCallback(&Server::callbackReadCommand);
		connections_.at(clientName).second->getStrBuf().reset(new boost::asio::streambuf);
		connections_.at(clientName).second->resetStrBuf();
		connections_.at(clientName).second->callAsyncRead();
		auto user{ connections_.find(clientName) };
		sendFriendList(user->second.second, std::to_string(clientId));
	}
}

void Server::writeCallback(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred)
{
	if (err) {
		closeClientConnection(connection);
		return;
	}
}

std::optional<std::vector<std::vector<std::string>>> Server::verificateHash(const std::string& hash)
{
	auto result{ databaseInstance.executeQuery("SELECT ID, LOGIN FROM CONTACTS WHERE TOKEN = '" + hash + "'") };
	if (result.empty()) {
		return std::nullopt;
	}
	return result;
}

void Server::loadUsers()
{
	auto result{ databaseInstance.executeQuery("SELECT ID,LOGIN FROM CONTACTS") };
	std::lock_guard<std::mutex> guard(mutex);
	for (auto& n : result) {
		std::unique_ptr<Client> tempClient{ new Client {n[1], std::stoull(n[0])} };
		connections_.insert({ std::to_string(tempClient->getId()), std::make_pair(std::move(tempClient), nullptr) });
	}
	std::thread trd{ &Server::pingClient, this };
	trd.detach();
}

Client& Server::findClientByConnection(std::shared_ptr<IConnectionHandler<Server>> connection)
{
	auto client{ std::find_if(connections_.begin(), connections_.end(), [connection](const auto& con) {return con.second.second == connection; }) };
	if (client != connections_.end()) {
		return *client->second.first.get();
	}
}

void Server::pingClient()
{
	while (true) {
		{
			std::lock_guard<std::mutex> guard(mutex);
			for (auto& client : connections_) {
				if (client.second.second != nullptr) {
					client.second.second->callWrite("Ping");
				}
			}
		}
		std::this_thread::sleep_for(chrono::seconds(60));
	}
}

void Server::callbackReadCommand(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred)
{
	if (err) {
		closeClientConnection(connection);
		return;
	}
	std::lock_guard<std::mutex> guard(mutex);
	Json::Value value;
	Json::Reader reader;
	reader.parse(connection->getData(), value);
	connection->resetStrBuf();
	auto client{ findClientByConnection(connection) };
	auto sender{ std::to_string(client.getId()) };
	auto receiver{ value["receiver"].asString()};
	if (value["command"] == SENDMESSAGE) {
		saveMessageToDatabase(sender, receiver, value["message"].asString());
		sendMessageToClient(value["receiver"].asString(), value["message"].asString(), sender);
		connection->callAsyncRead();
		return;
	}
	if (value["command"] == GETCHAT) {
		auto chatHistory{ getChatMessages(generateTableName(sender, receiver)) };
		sendChatHistory(sender, chatHistory);
		connection->callAsyncRead();
		return;
	}
}

void Server::sendMessageToClient(const std::string& to, const std::string& what, const std::string& from)
{
	auto foundUser{ connections_.find(to) };
	if (foundUser != connections_.end() && foundUser->second.first->onlineStatus) {
		Json::Value value;
		Json::FastWriter writer;
		value["command"] = SENDMESSAGE;
		value["receiver"] = std::to_string(foundUser->second.first->getId());
		value["message"] = what;
		value["sender"] = from;
		connections_.at(to).second->callWrite(writer.write(value));
	}
}

std::string Server::getJsonFriendList(const std::string& id)
{
	std::string tableName{ "FL_" + id };
	std::string query{ "SELECT * FROM " + tableName };
	auto result{ DatabaseHandler::getInstance().executeQuery(query) };
	if (result.empty()) {
		return "";
	}
	Json::Value finalValue;
	Json::FastWriter writer;
	for (auto row : result) {
		Json::Value tmpValue;
		tmpValue[row[0]] = row[1];
		tmpValue["lastMessage"] = getLastMessage(id, row[0]);
		finalValue.append(tmpValue);//
	}
	std::cout << finalValue.toStyledString();
	return writer.write(finalValue);
}

void Server::sendFriendList(std::shared_ptr<IConnectionHandler<Server>> connection, const std::string& userId)
{
	Json::Value value;
	Json::FastWriter writer;
	Json::Reader reader;
	value["data"] = getJsonFriendList(userId);
	value["command"] = FRIENDLIST;
	connection->callWrite(writer.write(value));
}

void Server::closeClientConnection(std::shared_ptr<IConnectionHandler<Server>> connection)
{
	std::lock_guard<std::mutex> guard(mutex);
	auto client{ findClientByConnection(connection) };
	connections_.at(std::to_string(client.getId())).first->onlineStatus = false;
	connections_.at(std::to_string(client.getId())).second = nullptr;
}

void Server::saveMessageToDatabase(const std::string& sender, const std::string& receiver, const std::string& msg)
{
	std::string tableName{ generateTableName(sender, receiver) };
	if(!DatabaseHandler::getInstance().tableExists(tableName)){
		createChatTable(tableName);
	}
	auto query{ "INSERT INTO " + tableName + " VALUES('" + sender + "', '" + receiver + "', '" + msg + "', Getdate())"};
	DatabaseHandler::getInstance().executeQuery(query);
}

Json::Value Server::getChatMessages(const std::string& chatName)
{
	if (!DatabaseHandler::getInstance().tableExists(chatName)) {
		createChatTable(chatName);
	}
	std::string query{ "SELECT * FROM " + chatName };
	auto result{ DatabaseHandler::getInstance().executeQuery(query) };
	if (result.empty()) {
		return {};
	}
	
	Json::Value finalValue;
	Json::FastWriter writer;
	for (auto row : result) {
		Json::Value value;
		value["sender"] = row[1];
		value["receiver"] = row[2];
		value["message"] = row[3];
		value["time"] = row[4];
		finalValue.append(value);
	}
	return finalValue;
}

void Server::sendChatHistory(const std::string& id, Json::Value& chatHistory)
{
	Json::Value value;
	Json::FastWriter writer;
	value["command"] = GETCHAT;
	value["data"] = chatHistory;
	connections_.at(id).second->callWrite(writer.write(value));
}

void Server::createChatTable(const std::string& tableName)
{
	std::string query{ "CREATE TABLE " + tableName + " (ID int NOT NULL IDENTITY(1,1) PRIMARY KEY, SENDER varchar(255) NOT NULL, RECEIVER varchar(255) NOT NULL, MESSAGE varchar(2048) NOT NULL, SENT_TIME DATETIME DEFAULT CURRENT_TIMESTAMP)" };
	DatabaseHandler::getInstance().executeQuery(query);
}

std::string Server::generateTableName(const std::string& sender, const std::string& receiver)
{
	auto first{ "CHAT_" + sender + "_" + receiver };
	auto second{ "CHAT_" + receiver + "_" + sender };
	return std::stoull(sender) > std::stoull(receiver) ? first : second;
}

std::string Server::getLastMessage(const std::string& sender, const std::string& receiver)
{
	auto tableName{ generateTableName(sender, receiver) };
	std::string query{ "SELECT TOP 1 MESSAGE FROM " + tableName + " ORDER BY ID DESC" };
	auto queryResult{ DatabaseHandler::getInstance().executeQuery(query) };
	if (!queryResult.empty()) {
		std::cout << queryResult[0][0];
	}
	return queryResult.empty() ? "" : queryResult[0][0];
}
