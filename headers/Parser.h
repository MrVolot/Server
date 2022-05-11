#pragma once
#include "json/json.h"
#include "Client.h"
#include "Constants.h"

class Parser {
	Json::Value value;
	Json::Reader reader;
	Parser();
public:
	std::string hash;
	static Parser& getInstance();
	void parseClientData(const std::string& str, unsigned long long id);
	Client* parseNewClientInfo(const std::string& str, unsigned long long id);
	Commands parseServerCommands(const std::string& str);
	Commands checkIncomingData(const std::string& str);
	bool verifyToken(const std::string& str);
};