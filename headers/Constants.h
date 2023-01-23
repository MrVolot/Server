#pragma once
#include <string>

static const std::string checkOnline{"status"};
static const std::string msg{ "sendMessage" };
static const std::string tokenVerification{ "tokenVerification" };

enum Commands {
	CHECK_ONLINE,
	SEND_MESSAGE,
	TOKEN_VERIFICATION,
	NONE
};

enum credentialsStatus {
	NEW_USER,
	WRONG_PASSWORD,
	VERIFIED,

	ERROR_
};

#define SENDMESSAGE 0x0001
#define FRIENDLIST 0x0002
#define GETCHAT 0x0003