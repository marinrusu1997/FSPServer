#pragma once
#include <memory>

#include "IUsersDB.h"
#include "ConfigSettings.h"
#include "KeysForConfigSettings.h"

inline constexpr const std::string_view SQLITE_USERS_DB_CONNECTION_STRING = "C:\\Users\\dimar\\SQLiteDatabases\\FSPClients.db";

class Configuration final
{
public:
	Configuration(const Configuration&) = delete;
	Configuration& operator=(const Configuration&) = delete;
	~Configuration(){}

	static Configuration&		get_instance() noexcept;
	void						set_users_db(std::unique_ptr<IUsersDB> udb_ptr) noexcept;
	IUsersDB*					get_users_db() const noexcept;
	auto&						get_config_settings() { return config_settings_; }

private:
	std::unique_ptr<IUsersDB>		users_db_ptr_;
	persistency::ConfigSettings		config_settings_;
	Configuration();
};

