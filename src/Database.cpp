#include "Database.h"
#include <odbc/PreparedStatement.h>
#include <odbc/ResultSet.h>
#include <odbc/Exception.h>
#include <fstream>
#include <iostream>

Database::Database() = default;

Database& Database::getInstance()
{
	static Database instance{};
	return instance;
}

void Database::connectDB()
{
	env = odbc::Environment::create();
	conn = env->createConnection();	
	try {
		conn->connect("SQLDB", "Server", "123");
		conn->setAutoCommit(false);
		odbc::PreparedStatementRef psInsert = conn->prepareStatement("USE Messenger");
		psInsert->executeQuery();
	}
	catch (odbc::Exception ex) {
		std::cout << ex.what();
	}
}

void Database::disconnectDB()
{
	conn->disconnect();
}

bool Database::searchUser(const std::string& nickName)
{
	if (nickName.empty()) {
		return false;
	}
	std::string sql{ "SELECT * FROM CONTACTS WHERE NICKNAME = '" + nickName + "'" };
	odbc::PreparedStatementRef psSelect{ conn->prepareStatement(sql.c_str()) };
	odbc::ResultSetRef rs = psSelect->executeQuery();
}

bool Database::verifyToken(const std::string& str)
{
	if (str.empty()) {
		return false;
	}
	std::string sql{ "SELECT TOKEN FROM CONTACTS WHERE TOKEN = '" + str + "'"};
	odbc::PreparedStatementRef psSelect{ conn->prepareStatement(sql.c_str()) };
	odbc::ResultSetRef rs = psSelect->executeQuery();
	while (rs->next())
	{
		std::cout << rs->getInt(1) << ", " << rs->getString(2) << std::endl;
	}
}