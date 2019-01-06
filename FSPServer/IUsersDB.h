#pragma once
#include <string>
#include <string_view>

class IUsersDB
{
protected:
	IUsersDB() {};
public:
	enum class status_code : int8_t {
		user_credentials_ok,
		user_not_found,
		user_pswd_incorrect,

		registered_succes,
		user_already_registered,

		user_removed,
		user_failed_to_remove,
	
		internal_error};

	struct user {
		std::string_view nickname;
		std::string_view password;
	};

	virtual ~IUsersDB(){}
	/*
		Return values:
			internal_error
			user_not_found
			user_pswd_incorrect
			user_credentials_ok
	*/
	virtual status_code retrieve_user(user const& user) const noexcept = 0;
	/*
		Return values:
			internal error
			user_already_registered
			registered_succes
	*/
	virtual status_code register_user(user const& user) const noexcept = 0;
	/*
		Return values:
			internal_error
			user_failed_to_remove
			user_removed
	*/
	virtual status_code erase_user(user const& user) const noexcept = 0;

	static std::string_view to_string(status_code code) noexcept;
};

