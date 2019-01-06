#include "IUsersDB.h"

std::string_view IUsersDB::to_string(IUsersDB::status_code code) noexcept
{
	switch (code)
	{
	case IUsersDB::status_code::user_credentials_ok:
		return "User credentials ok";
	case IUsersDB::status_code::user_not_found:
		return "User wasn't found";
	case IUsersDB::status_code::user_pswd_incorrect:
		return "User password is not correct";
	case IUsersDB::status_code::registered_succes:
		return "User was registered with succes";
	case IUsersDB::status_code::user_already_registered:
		return "User is already registered";
	case IUsersDB::status_code::user_removed:
		return "User was removed successfully";
	case IUsersDB::status_code::user_failed_to_remove:
		return "User was not removed";
	case IUsersDB::status_code::internal_error:
		return "Internal error";
	default:
		return "UNKNOWN STATUS CODE";
	}
}