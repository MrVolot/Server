#include "Parser.h"
#include "server.h"
#include "Database.h"

Parser::Parser() = default;

Parser& Parser::getInstance()
{
	static Parser instance{};
	return instance;
}

void Parser::parseClientData(const std::string& str, unsigned long long id)
{
	reader.parse(str.c_str(), value);
	auto result{ parseServerCommands(value["command"].asString()) };
	switch (result) {
	case Commands::CHECK_ONLINE: 
	{
		Server::sendOnlineResponse(id);
		break;
	}
	case Commands::SEND_MESSAGE:
	{
		Server::writer(str, std::stoull(value["to"].asString()), id);
	}
	}
}

Client* Parser::parseNewClientInfo(const std::string& str, unsigned long long id)
{
	reader.parse(str.c_str(), value);
	//auto status{ Database::getInstance().insertInfo(value["login"].asCString(), value["password"].asCString()) };
	//switch (status) {
	//case credentialsStatus::NEW_USER:
	//{
	//	auto tmp{ new Client{ value["login"].asCString(), id } };
	//	return tmp;
	//}
	//case credentialsStatus::VERIFIED:
	//{

	//}
	//case credentialsStatus::WRONG_PASSWORD: 
	//{

	//}
	//case credentialsStatus::ERROR_: 
	//{

	//}

	//}
	return new Client{ value["login"].asCString(), id };
}

Commands Parser::parseServerCommands(const std::string& str)
{
	if (!str.compare(checkOnline)) {
		return Commands::CHECK_ONLINE;
	}
	if (!str.compare(msg)) {
		return Commands::SEND_MESSAGE;
	}
	if (!str.compare(tokenVerification)) {
		return Commands::TOKEN_VERIFICATION;
	}
	return Commands::NONE;
}

Commands Parser::checkIncomingData(const std::string& str)
{
	reader.parse(str.c_str(), value);
	return parseServerCommands(value["command"].asString());
}

bool Parser::verifyToken(const std::string& str)
{
	reader.parse(str.c_str(), value);
	hash = value["token"].asString();
	return Database::getInstance().verifyToken(hash);
}
