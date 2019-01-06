#include "confirmator.h"
#include "message.h"
#include "fsp_responses.h"
#include "fsp_commands.h"

using namespace fsp::protocol::commands;
using namespace fsp::protocol::responses;
using namespace fsp::protocol::headers;

_NODISCARD const char * confirmator::confirm(fsp::protocol::message::message const& req) noexcept
{
	if (req.command() == SIGNIN)
		return SIGNIN_CMD_ACCEPTED;
	if (req.command() == SIGNUP)
		return SIGNUP_CMD_ACCEPTED;
	if (req.command() == DELETE_ACCOUNT)
		return DELETE_ACCOUNT_CMD_ACCEPTED;
	if (req.command() == LOGOUT)
		return LOGOUT_CMD_ACCEPTED;

	if (req.command() == PUSH_FILES)
		return PUSH_FILES_CMD_ACCEPTED;

	if (req.command() == QUERRY)
		return QUERRY_CMD_ACCEPTED;
	
	return nullptr;
}