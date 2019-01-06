#pragma once
#include "IUsersDB.h"
#include <SQLAPI.h>

class SQLiteDB final : public IUsersDB
{
public:
	SQLiteDB(std::string_view dbname, std::string_view username, std::string_view pswd) noexcept;
	SQLiteDB(SQLiteDB const&) = delete;
	SQLiteDB& operator=(SQLiteDB const&) = delete;


	status_code retrieve_user(user const& user) const noexcept;
	status_code register_user(user const& user) const noexcept;
	status_code erase_user(user const& user) const noexcept;

private:
	mutable SAConnection	connection_;
};

