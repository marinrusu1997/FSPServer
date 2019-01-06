#pragma once
#include "fsp_commands.h"
#include "scheduler_priorities.h"
#include <map>

using namespace fsp::protocol::commands;

namespace fsp::protocol::impl {

	static const std::map<std::string_view, scheduler_priority, std::less<> > cmd_priorities{
		{SIGNIN, scheduler_priority::high},
		{SIGNUP, scheduler_priority::high},
		{DELETE_ACCOUNT, scheduler_priority::low},
		{LOGOUT, scheduler_priority::very_high},
		{PUSH_FILES, scheduler_priority::medium},
		{UPDATE_FILE_LIST_NOTIFICATION, scheduler_priority::high},
		{USER_LOGGED_OUT_NOTIFICATION, scheduler_priority::very_high},
		{REMOVE_FILES, scheduler_priority::medium},
		{DOWNLOAD_FILE, scheduler_priority::low},
		{REPLY_NOTIFICATION, scheduler_priority::very_low}
	};
}