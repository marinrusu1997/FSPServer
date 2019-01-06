#include "transition_validator.h"
#include "message.h"
#include "client.h"
#include "fsp_commands.h"
#include "fsp_responses.h"

using namespace fsp::protocol::commands;
using namespace fsp::protocol::responses;
using namespace fsp::impl::server::models;

_NODISCARD const char * transition_validator::validate(fsp::protocol::message::message const& req, fsp::impl::server::models::client const& client_) noexcept
{
	const auto& current_command = req.command();

	switch (client_.state().load())
	{
	case client::State::connected:
	{
		if (current_command != SIGNIN && current_command != SIGNUP)
			return INVALID_CMD_WHILE_IN_CONN_STATE;
	}
	break;
	case client::State::logged:
	{
		if (current_command != PUSH_FILES &&
			current_command != DELETE_ACCOUNT &&
			current_command != LOGOUT)
			return INVALID_CMD_WHILE_IN_LOGGED_STATE;
	}
	break;
	case client::State::processing:
	{
		if (current_command != DELETE_ACCOUNT &&
			current_command != LOGOUT &&
			current_command != QUERRY &&
			current_command != REPLY)
			return INVALID_CMD_WHILE_IN_PROCESSING_STATE;
	}
	break;
	}

	return nullptr;
}