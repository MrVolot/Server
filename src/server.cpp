#include "server.h"

#include <iostream>
#include "certificateUtils/certificateUtils.h"

Server::Server(io_service& service) : 
	service_{ service },
	acceptor_{ service, ip::tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), 10690) },
	databaseInstance{ DatabaseHandler::getInstance() },
	ssl_context_ {boost::asio::ssl::context::sslv23}
{
	databaseInstance.connectDB("Server", "123");
	loadUsers();

	std::shared_ptr<EVP_PKEY> private_key = certificateUtils::generate_private_key(2048);
	std::shared_ptr<X509> certificate = certificateUtils::generate_self_signed_certificate("Server", private_key.get(), 365);

	// Load the CA certificate into memory
	std::shared_ptr<X509> ca_cert = certificateUtils::load_ca_certificate();

	X509_STORE* cert_store = SSL_CTX_get_cert_store(ssl_context_.native_handle());
	X509_STORE_add_cert(cert_store, ca_cert.get());

	ssl_context_.use_private_key(boost::asio::const_buffer(certificateUtils::private_key_to_pem(private_key.get()).data(),
		certificateUtils::private_key_to_pem(private_key.get()).size()),
		boost::asio::ssl::context::pem);
	ssl_context_.use_certificate(boost::asio::const_buffer(certificateUtils::certificate_to_pem(certificate.get()).data(),
		certificateUtils::certificate_to_pem(certificate.get()).size()),
		boost::asio::ssl::context::pem);
	ssl_context_.set_verify_mode(boost::asio::ssl::verify_peer);
	ssl_context_.set_verify_callback(
		[](bool preverified, boost::asio::ssl::verify_context& ctx) {
			return certificateUtils::custom_verify_callback(preverified, ctx, "ServerClient");
		});
	ssl_context_.set_options(boost::asio::ssl::context::default_workarounds |
		boost::asio::ssl::context::no_sslv2 |
		boost::asio::ssl::context::no_sslv3 |
		boost::asio::ssl::context::single_dh_use);

	startAccept();
}

void Server::handleAccept(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err)
{
	if (!err) {
		connection->setAsyncReadCallback(&Server::readConnection);
		connection->setWriteCallback(&Server::writeCallback);
		connection->callAsyncHandshake();
	}
	startAccept();
}

void Server::startAccept()
{
	auto connection{ std::make_shared<HttpsConnectionHandler<Server, ConnectionHandlerType::SERVER>>(service_, *this, ssl_context_) };
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
			std::unique_ptr<Client> tempClient{ new Client {clientName, std::stoull(result.value()[0][0]), result.value()[0][1]} };
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
	auto result{ databaseInstance.executeQuery("SELECT ID, LOGIN, PUBLIC_KEY FROM CONTACTS") };
	std::lock_guard<std::mutex> guard(mutex);
	for (auto& n : result) {
		std::unique_ptr<Client> tempClient{ new Client {n[1], std::stoull(n[0]), n[2]}};
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
	auto receiver{ value["receiver"].asString() };
	if (value["command"] == SENDMESSAGE) {
		auto messageText{ value["message"].asString() };
		auto messageGuid{ value["messageGuid"].asString() };
		auto sentTime { saveMessageToDatabase(messageGuid, sender, receiver, messageText) };
		MessageInfo msgInfo{ messageGuid, messageText, sentTime, sender, receiver };
		sendMessageToClient(msgInfo);
		connection->callAsyncRead();
		return;
	}
	if (value["command"] == GETCHAT) {
		auto chatHistory{ getChatMessages(generateTableName(sender, receiver)) };
		sendChatHistory(sender, chatHistory);
		connection->callAsyncRead();
		return;
	}
	if (value["command"] == TRY_GET_CONTACT_BY_LOGIN) {
		auto possibleContacts{ tryGetContactInfo(value["LOGIN"].asString()) };
		sendPossibleContactsInfo(connection, possibleContacts.has_value() ? possibleContacts.value() : new Json::Value());
		return;
	}
	if (value["command"] == DELETE_MESSAGE) {
		deleteMessageById(sender, value["receiverId"].asString(), value["messageGuid"].asString());
		connection->callAsyncRead();
		return;
	}
}

void Server::sendMessageToClient(const MessageInfo& messageInfo)
{
	auto foundUser{ connections_.find(messageInfo.receiverId) };
	if (foundUser != connections_.end() && foundUser->second.first->onlineStatus) {
		Json::Value value;
		Json::FastWriter writer;
		value["command"] = SENDMESSAGE;
		value["messageGuid"] = messageInfo.messageId;
		value["receiver"] = std::to_string(foundUser->second.first->getId());
		value["message"] = messageInfo.messageText;
		value["sender"] = messageInfo.senderId;
		value["time"] = messageInfo.sentTime;
		connections_.at(messageInfo.receiverId).second->callWrite(writer.write(value));
	}
}

Json::Value Server::getJsonFriendList(const std::string& id)
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
		tmpValue["id"] = std::stoull(row[0]);
		tmpValue["name"] = row[1];
		tmpValue["lastMessage"] = getLastMessage(id, row[0]);
		tmpValue["publicKey"] = getPublicKey(row[0]);
		finalValue.append(tmpValue);
	}
	return finalValue;
}

std::string Server::getPublicKey(const std::string& id)
{
	std::string query{ "SELECT PUBLIC_KEY FROM CONTACTS WHERE ID = '" + id + "'"};
	auto result{ DatabaseHandler::getInstance().executeQuery(query) };
	if (result.empty()) {
		return "";
	}
	return result[0][0];
}

void Server::sendFriendList(std::shared_ptr<IConnectionHandler<Server>> connection, const std::string& userId)
{
	Json::Value value;
	Json::FastWriter writer;
	Json::Reader reader;
	value["command"] = FRIENDLIST;
	value["personalId"] = std::stoull(userId);
	value["data"] = getJsonFriendList(userId);
	connection->callWrite(writer.write(value));
}

void Server::closeClientConnection(std::shared_ptr<IConnectionHandler<Server>> connection)
{
	std::lock_guard<std::mutex> guard(mutex);
	auto client{ findClientByConnection(connection) };
	connections_.at(std::to_string(client.getId())).first->onlineStatus = false;
	connections_.at(std::to_string(client.getId())).second = nullptr;
}

std::string Server::saveMessageToDatabase(const std::string& messageGuid, const std::string& sender, const std::string& receiver, const std::string& msg)
{
	verifyFriendsConnection(sender, receiver);
	std::string tableName{ generateTableName(sender, receiver) };
	if (!DatabaseHandler::getInstance().tableExists(tableName)) {
		createChatTable(tableName);
	}
	auto query{ "INSERT INTO " + tableName + " OUTPUT INSERTED.SENT_TIME VALUES(?, ?, ?, ?, Getdate())" };
	auto result{ DatabaseHandler::getInstance().executeWithPreparedStatement(query, {messageGuid, sender, receiver, msg})};
	result.next();
	return result.get<std::string>(0);
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
		value["messageGuid"] = row[1];
		value["sender"] = row[2];
		value["receiver"] = row[3];
		value["message"] = row[4];
		value["time"] = row[5];
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
	std::string query{ "CREATE TABLE " + tableName + " (ID int NOT NULL IDENTITY(1,1) PRIMARY KEY, GUID varchar(36) NOT NULL UNIQUE, SENDER varchar(255) NOT NULL, RECEIVER varchar(255) NOT NULL, MESSAGE varchar(2048) NOT NULL, SENT_TIME DATETIME DEFAULT CURRENT_TIMESTAMP)" };
	DatabaseHandler::getInstance().executeQuery(query);
}

std::string Server::generateTableName(const std::string& sender, const std::string& receiver)
{
	auto first{ "CHAT_" + sender + "_" + receiver };
	auto second{ "CHAT_" + receiver + "_" + sender };
	return std::stoull(sender) > std::stoull(receiver) ? first : second;
}

Json::Value Server::getLastMessage(const std::string& sender, const std::string& receiver)
{
	Json::Value value;
	auto tableName{ generateTableName(sender, receiver) };
	if (DatabaseHandler::getInstance().tableExists(tableName)) {
		std::string query{ "SELECT TOP 1 MESSAGE, SENDER FROM " + tableName + " ORDER BY ID DESC" };
		auto queryResult{ DatabaseHandler::getInstance().executeQuery(query) };
		if (!queryResult.empty()) {
			value["message"] = queryResult[0][0];
			value["sender"] = std::stoull(queryResult[0][1]);
		}
	}
	return value;
}

std::optional<Json::Value> Server::tryGetContactInfo(const std::string& login)
{
	//Add name retrieval
	Json::Value finalValue;
	if (!login.empty()) {
		std::string query{ "SELECT ID, LOGIN FROM CONTACTS WHERE LOGIN LIKE '%" + login + "%'" };
		auto queryResult{ DatabaseHandler::getInstance().executeQuery(query) };
		if (queryResult.empty()) {
			return std::nullopt;
		}
		Json::FastWriter writer;
		for (auto row : queryResult) {
			Json::Value value;
			value["ID"] = std::stoull(row[0]);
			value["LOGIN"] = row[1];
			finalValue.append(value);
		}
	}
	return finalValue;
}

void Server::sendPossibleContactsInfo(std::shared_ptr<IConnectionHandler<Server>> connection, const Json::Value& value)
{
	Json::FastWriter writer;
	Json::Value tmpValue;
	tmpValue["command"] = TRY_GET_CONTACT_BY_LOGIN;
	tmpValue["data"] = value;
	connection->callWrite(writer.write(tmpValue));
	connection->callAsyncRead();
}

void Server::insertFriendIfNeeded(const std::string& tableName, std::pair<const std::string&, const std::string&> value)
{
	std::string query{ "IF NOT EXISTS (SELECT * FROM " + tableName + " WHERE ID = ?) BEGIN INSERT INTO " + tableName + " VALUES(?, ?) END" };
	DatabaseHandler::getInstance().executeWithPreparedStatement(query, { value.first, value.first, value.second});
}

void Server::verifyFriendsConnection(const std::string& sender, const std::string& receiver)
{
	auto user1{ connections_.find(sender) };
	auto user2{ connections_.find(receiver) };
	std::string table1{ "FL_" + sender };
	std::string table2{ "FL_" + receiver };
	if (!DatabaseHandler::getInstance().tableExists(table1)) {
		
		std::string query{ "CREATE TABLE " + table1 + " (ID int NOT NULL PRIMARY KEY, Name varchar(255) NOT NULL)" };
		DatabaseHandler::getInstance().executeQuery(query);
	}
	insertFriendIfNeeded(table1, { std::to_string(user2->second.first->getId()), user2->second.first->getName() });
	if (!DatabaseHandler::getInstance().tableExists(table2)) {
		
		std::string query{ "CREATE TABLE " + table2 + " (ID int NOT NULL PRIMARY KEY, Name varchar(255) NOT NULL)" };
		DatabaseHandler::getInstance().executeQuery(query);
	}
	//TODO send some info that new message from new person appeared , in order to create chat widget for receiver
	//it friend didn't exist and was inserted, we must send info to create chat
	//otherwise we do not care, since this chat already existed
	insertFriendIfNeeded(table2, { std::to_string(user1->second.first->getId()), user1->second.first->getName() });
}

void Server::deleteMessageById(const std::string& sender, const std::string& receiver, const std::string& messageId)
{
	auto tableName{ generateTableName(sender, receiver) };
	std::string query{ "DELETE FROM " + tableName + " WHERE [GUID] = ?" };
	DatabaseHandler::getInstance().executeWithPreparedStatement(query, {messageId});

	Json::Value value;
	Json::FastWriter writer;
	value["command"] = DELETE_MESSAGE;
	value["messageGuid"] = messageId;
	value["receiver"] = receiver;
	value["sender"] = sender;
	connections_.at(receiver).second->callWrite(writer.write(value));
}
