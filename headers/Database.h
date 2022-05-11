#pragma once
#include "Constants.h"
#include <odbc/Connection.h>
#include <odbc/Environment.h>
#include <string>

class Database {
	odbc::ConnectionRef conn;
	odbc::EnvironmentRef env;
	Database();
public:
	static Database& getInstance();
	void connectDB();
	void disconnectDB();
	bool searchUser(const std::string& nickName);
	bool verifyToken(const std::string& str);
};