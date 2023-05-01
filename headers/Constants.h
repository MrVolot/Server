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

struct MessageInfo {
	std::string messageId;
	std::string messageText;
	std::string sentTime;
	std::string senderId;
	std::string receiverId;
};

#define SENDMESSAGE 0x0001
#define FRIENDLIST 0x0002
#define GETCHAT 0x0003
#define TRY_GET_CONTACT_BY_LOGIN 0x0004
#define DELETE_MESSAGE 0x0005
#define DELETE_ACCOUNT 0x0006
#define REQUEST_PUBLIC_KEY 0x0007
#define EMAIL_ADDITION 0x0008
#define CODE_VERIFICATION 0x0009
#define DISABLE_EMAIL_AUTH 0x000A
#define SEND_FILE 0x000B