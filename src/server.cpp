#include "server.h"

#include <iostream>
#include "certificateUtils/certificateUtils.h"
#include <random>

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
	Json::Value value;
	Json::Reader reader;
	reader.parse(connection->getData(), value);
	if (auto result{ verificateHash(value["hash"].asString())}; result != std::nullopt) {
		std::string clientId{ result.value()[0][0] };
		if (!value["publicKey"].isNull()) {
			saveUserPublicKey(clientId, value["publicKey"].asString());
		}
		if (auto user{ connections_.find(clientId) }; user != connections_.end()) {
			user->second.second = std::move(connection);
			user->second.first->onlineStatus = true;
		}
		else {
			std::unique_ptr<Client> tempClient{ new Client {result.value()[0][1], std::stoull(clientId), result.value()[0][2]} };
			tempClient->onlineStatus = true;
			connections_.insert({ std::to_string(tempClient->getId()), std::make_pair(std::move(tempClient),connection) });
		}
		connections_.at(clientId).second->setAsyncReadCallback(&Server::callbackReadCommand);
		connections_.at(clientId).second->getStrBuf().reset(new boost::asio::streambuf);
		connections_.at(clientId).second->resetStrBuf();
		connections_.at(clientId).second->callAsyncRead();
		auto user{ connections_.find(clientId) };
		sendFriendList(user->second.second, clientId);
	}
}

std::string Server::getUserPublicKey(const std::string& id)
{
	auto query{ "SELECT PUBLIC_KEY FROM " + ContactsTableName + " WHERE ID = ? " };
	auto result{ DatabaseHandler::getInstance().executeWithPreparedStatement(query, {id}) };
	result.next();
	//TODO: Verify if it is not null. If it is null and we try to get values, it will throw.
	return result.get<std::string>(0);
}

void Server::writeCallback(std::shared_ptr<IConnectionHandler<Server>> connection, const boost::system::error_code& err, size_t bytes_transferred)
{
	if (err) {
		closeClientConnection(connection);
		return;
	}
}

void Server::saveUserPublicKey(const std::string& id, const std::string& publicKey)
{
	auto query{ "UPDATE " + ContactsTableName + " SET PUBLIC_KEY = ? WHERE ID = ? " };
	DatabaseHandler::getInstance().executeWithPreparedStatement(query, { publicKey, id });
}

std::optional<std::vector<std::vector<std::string>>> Server::verificateHash(const std::string& hash)
{
	auto result{ databaseInstance.executeQuery("SELECT ID, LOGIN, PUBLIC_KEY FROM " + ContactsTableName + " WHERE TOKEN = '" + hash + "'") };
	if (result.empty()) {
		return std::nullopt;
	}
	return result;
}

void Server::processPublicKeyRetrieval(std::shared_ptr<IConnectionHandler<Server>> connection, const std::string& id)
{
	auto publicKey{ getUserPublicKey(id) };
	Json::Value tmpValue{};
	tmpValue["command"] = REQUEST_PUBLIC_KEY;
	tmpValue["id"] = id;
	tmpValue["userPublicKey"] = publicKey;
	Json::StreamWriterBuilder writer;
	auto sendValue{ Json::writeString(writer, tmpValue) };
	connection->callWrite(sendValue);
	connection->callAsyncRead();
}

void Server::loadUsers()
{
	auto result{ databaseInstance.executeQuery("SELECT ID, LOGIN, PUBLIC_KEY FROM " + ContactsTableName) };
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

std::string Server::getUserEmailById(const std::string& id)
{
	std::string query{ "SELECT EMAIL FROM " + ContactsTableName + " WHERE ID = " + id };
	auto result{ DatabaseHandler::getInstance().executeQuery(query) };
	if (result.empty()) {
		return "";
	}
	return result[0][0];
}

void Server::disableEmailAuthById(const std::string& id)
{
	auto query = "UPDATE " + ContactsTableName + " SET AUTHENTICATION_ENABLED = 0, EMAIL = NULL, AUTHENTICATION_CODE = NULL WHERE ID = ?";
	DatabaseHandler::getInstance().executeWithPreparedStatement(query, { id });
}

void Server::sendFileToClient(const std::string receiverId, const std::string senderId, const std::string fileStream, const std::string fileName)
{
	auto foundUser{ connections_.find(receiverId) };
	if (foundUser != connections_.end() && foundUser->second.first->onlineStatus) {
		Json::Value value;
		Json::FastWriter writer;
		value["command"] = SEND_FILE;
		value["receiver"] = std::to_string(foundUser->second.first->getId());
		value["fileStream"] = fileStream;
		value["fileName"] = fileName;
		value["sender"] = senderId;
		connections_.at(receiverId).second->callWrite(writer.write(value));
	}
}

void Server::changePasswordById(const std::string& id, const std::string& newPassword)
{
	auto query = "UPDATE " + ContactsTableName + " SET PASSWORD = '" + newPassword + "' WHERE ID = " + id;
	DatabaseHandler::getInstance().executeDbcQuery(query);
}

void Server::changeAvatarById(const std::string& id, const std::string& photoStream)
{
	auto query = "UPDATE " + ContactsTableName + " SET PHOTOSTREAM = '" + photoStream + "' WHERE ID = " + id;
	DatabaseHandler::getInstance().executeDbcQuery(query);
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
	auto vcheck{ value["command"].asInt() };
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
	if (value["command"] == DELETE_ACCOUNT) {
		deleteAccountById(value["id"].asString());
		connection->callWrite("close connection");
		return;
	}
	if (value["command"] == REQUEST_PUBLIC_KEY) {
		processPublicKeyRetrieval(connection, value["id"].asString());
		return;
	}
	if (value["command"] == EMAIL_ADDITION) {
		setUserEmailForVerification(value["email"].asString(), sender);
		connection->callAsyncRead();
		return;
	}
	if (value["command"] == CODE_VERIFICATION) {
		auto verificationResult{ verifyEmailCode(sender, value["verCode"].asString()) };
		Json::Value value;
		Json::FastWriter writer;
		value["command"] = CODE_VERIFICATION;
		value["verResult"] = verificationResult;
		connection->callWrite(writer.write(value));
		connection->callAsyncRead();
		return;
	}
	if (value["command"] == DISABLE_EMAIL_AUTH) {
		disableEmailAuthById(sender);
		connection->callAsyncRead();
		return;
	}
	if (value["command"] == SEND_FILE) {
		sendFileToClient(value["receiver"].asString(), sender, value["fileStream"].asString(), value["fileName"].asString());
		connection->callAsyncRead();
		return;
	}
	if (value["command"] == EDIT_MESSAGE) {
		editMessageById(sender, value["receiverId"].asString(), value["newText"].asString(), value["messageGuid"].asString());
		connection->callAsyncRead();
		return;
	}
	if (value["command"] == CHANGE_PASSWORD) {
		changePasswordById(sender, value["newPassword"].asString());
		connection->callAsyncRead();
		return;
	}
	if (value["command"] == UPDATE_AVATAR) {
		// For now Server will not handle this
		//changeAvatarById(sender, value["photoStream"].asString());
		connection->callAsyncRead();
		return;
	}
}

void Server::setUserEmailForVerification(const std::string& email, const std::string& userId)
{
	auto authCode{ generateUniqueCode() };
	emailHandler.sendEmail(email, authCode);
	auto query = "UPDATE " + ContactsTableName + " SET AUTHENTICATION_CODE = ?, EMAIL = ? WHERE ID = ?";
	DatabaseHandler::getInstance().executeWithPreparedStatement(query, { authCode, email, userId });
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
		value["senderName"] = connections_.at(messageInfo.senderId).first->getName();
		connections_.at(messageInfo.receiverId).second->callWrite(writer.write(value));
	}
}

Json::Value Server::getJsonFriendList(const std::string& id)
{
	std::string tableName{ "FL_" + id };
	std::string query{ "SELECT * FROM " + DbNamePrefix + tableName };
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
	std::string query{ "SELECT PUBLIC_KEY FROM " + ContactsTableName + " WHERE ID = '" + id + "'"};
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
	value["personalEmail"] = getUserEmailById(userId);
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
	auto query{ "INSERT INTO " + tableName + " OUTPUT INSERTED.SENT_TIME VALUES('" + messageGuid + "', " + sender + ", " + receiver + ", '" + msg + "', Getdate())" };
	auto result{ DatabaseHandler::getInstance().executeDbcQuery(query)};
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
	std::string query{
	"CREATE TABLE " + tableName + " ("
	"ID int NOT NULL IDENTITY(1,1) PRIMARY KEY, "
	"GUID varchar(36) NOT NULL UNIQUE, "
	"SENDER int NOT NULL, "
	"RECEIVER int NOT NULL, "
	"MESSAGE varchar(2048) NOT NULL, "
	"SENT_TIME DATETIME DEFAULT CURRENT_TIMESTAMP, "
	"FOREIGN KEY (SENDER) REFERENCES " + ContactsTableName + "(ID), "
	"FOREIGN KEY (RECEIVER) REFERENCES " + ContactsTableName + "(ID))"
	};
	DatabaseHandler::getInstance().executeQuery(query);
}

std::string Server::generateTableName(const std::string& sender, const std::string& receiver)
{
	auto first{ ChatTableNamePrefix + sender + "_" + receiver };
	auto second{ ChatTableNamePrefix + receiver + "_" + sender };
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
		std::string query{ "SELECT ID, LOGIN FROM " + ContactsTableName + " WHERE LOGIN LIKE '%" + login + "%'" };
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
	std::string query{ "IF NOT EXISTS (SELECT * FROM " + tableName + " WHERE ID = " + value.first + ") BEGIN INSERT INTO " + tableName + " VALUES("+ value.first + " , '" + value.second + "' ) END" };
	DatabaseHandler::getInstance().executeDbcQuery(query);
}

void Server::verifyFriendsConnection(const std::string& sender, const std::string& receiver)
{
	auto user1{ connections_.find(sender) };
	auto user2{ connections_.find(receiver) };
	std::string table1{ DbNamePrefix + "FL_" + sender };
	std::string table2{ DbNamePrefix + "FL_" + receiver };
	if (!DatabaseHandler::getInstance().tableExists(table1)) {
		std::string query{ "CREATE TABLE " + table1 + "(ID int NOT NULL PRIMARY KEY, Name varchar(255) NOT NULL, CONSTRAINT FK_FL_" + sender + "_Contacts FOREIGN KEY(ID) REFERENCES " + ContactsTableName + "(ID))" };
		DatabaseHandler::getInstance().executeDbcQuery(query);
	}
	insertFriendIfNeeded(table1, { std::to_string(user2->second.first->getId()), user2->second.first->getName() });
	if (!DatabaseHandler::getInstance().tableExists(table2)) {
		std::string query{ "CREATE TABLE " + table2 + "(ID int NOT NULL PRIMARY KEY, Name varchar(255) NOT NULL, CONSTRAINT FK_FL" + receiver + "_Contacts FOREIGN KEY(ID) REFERENCES " + ContactsTableName + "(ID))" };
		DatabaseHandler::getInstance().executeDbcQuery(query);
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
	if (connections_.at(receiver).second != nullptr) {
		connections_.at(receiver).second->callWrite(writer.write(value));
	}
}

void Server::editMessageById(const std::string& sender, const std::string& receiver, const std::string& newText, const std::string& messageId)
{
	auto tableName{ generateTableName(sender, receiver) };
	std::string query{ "UPDATE " + tableName + " SET MESSAGE = '" + newText + "' WHERE [GUID] = '" + messageId + "'"};
	DatabaseHandler::getInstance().executeDbcQuery(query);

	Json::Value value;
	Json::FastWriter writer;
	value["command"] = EDIT_MESSAGE;
	value["messageGuid"] = messageId;
	value["receiverId"] = receiver;
	value["senderId"] = sender;
	value["newText"] = newText;

	if (connections_.at(receiver).second != nullptr) {
		connections_.at(receiver).second->callWrite(writer.write(value));
	}
}

void Server::deleteAccountById(const std::string& id)
{
	std::string query = "DELETE FROM " + AuthTableName + " WHERE USERID = " + id;
	DatabaseHandler::getInstance().executeDbcQuery(query);
	query = "DELETE FROM " + ContactsTableName + " WHERE ID = " + id;
	DatabaseHandler::getInstance().executeDbcQuery(query);
	query = "DROP TABLE " + DbNamePrefix + "FL_" + id;
	DatabaseHandler::getInstance().executeDbcQuery(query);
}

std::string Server::generateUniqueCode() {
	std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	int length = 8; // Length of the random string

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dist(0, charset.size() - 1);

	std::string random_string;
	for (int i = 0; i < length; ++i) {
		random_string += charset[dist(gen)];
	}

	return random_string;
}

bool Server::verifyEmailCode(const std::string& id, const std::string& code) 
{
	std::string query{ "SELECT AUTHENTICATION_CODE FROM " + ContactsTableName + " WHERE ID = " + id };
	auto result{ DatabaseHandler::getInstance().executeQuery(query) };
	if (!result.empty() && result[0][0] == code) {
		query = "UPDATE " + ContactsTableName + " SET AUTHENTICATION_ENABLED = 1 WHERE ID = ?";
		DatabaseHandler::getInstance().executeWithPreparedStatement(query, { id });
		return true;
	}
	return false;
}