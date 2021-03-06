#include "Server.h"
#include "Configuration.h"
#include "SQLiteDB.h"
#include <iostream>

int main()
{
	Configuration::get_instance().set_users_db(std::move(std::make_unique<SQLiteDB>(SQLITE_USERS_DB_CONNECTION_STRING, "", "")));
	Server server((uint16_t)std::stoi(Configuration::get_instance().get_config_settings().try_get(
		persistency::keys::common::SERV_PORT, { "8070" }).front()));
	server.run();

	std::string cmd;
	while (cmd != "exit") 
	{
		std::cout << "Command$:";
		std::cin >> cmd;
	}

	return EXIT_SUCCESS;
}