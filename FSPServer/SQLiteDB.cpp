#include "SQLiteDB.h"
#include <iostream>



SQLiteDB::SQLiteDB(std::string_view dbname, std::string_view username, std::string_view pswd) noexcept :
	connection_()
{
	try {
		connection_.Connect(dbname.data(), username.data(), pswd.data(), SA_SQLite_Client);
		connection_.setAutoCommit(SA_AutoCommitOn);
		connection_.setIsolationLevel(SAIsolationLevel_t::SA_Serializable);
	}
	catch (SAException const& e)
	{
		std::cerr << e.ErrText().GetMultiByteChars() << std::endl;
	}
	catch(...)
	{
		std::cerr << "Exception occured while instantiating SQLiteDB object" << std::endl;
	}
}

SQLiteDB::status_code SQLiteDB::retrieve_user(user const& user) const noexcept
{
	try {
		if (!connection_.isConnected() || !connection_.isAlive())
			return status_code::internal_error;
		SACommand command(&this->connection_, 
			"select Clients.nickname,Clients.password from Clients"
			"		where Clients.nickname == :1");
		command << user.nickname.data();
		command.Execute();
		if (!command.isExecuted())
			return status_code::internal_error;
		if (!command.isResultSet())
			return status_code::user_not_found;
		if (command.FetchNext()) {
			if (user.password.compare(command[2].asString().GetMultiByteChars()) == 0)
				return status_code::user_credentials_ok;
			else
				return status_code::user_pswd_incorrect;
		}
		else
			return status_code::user_not_found;
		
	}
	catch (SAException const& e)
	{
		std::cerr << e.ErrText().GetMultiByteChars() << std::endl;
		return status_code::internal_error;
	}
	catch (...)
	{
		std::cerr << "Exception occured in SQLiteDB::retrieve_user function" << std::endl;
		return status_code::internal_error;
	}
}

SQLiteDB::status_code SQLiteDB::register_user(user const& user) const noexcept
{
	try {
		if (!connection_.isConnected() || !connection_.isAlive())
			return status_code::internal_error;
		SACommand retrieveCommand(&this->connection_,
			"select Clients.nickname from Clients"
			"		where Clients.nickname == :1");
		retrieveCommand << user.nickname.data();
		retrieveCommand.Execute();
		if (!retrieveCommand.isExecuted())
			return status_code::internal_error;
		if (!retrieveCommand.isResultSet())
			return status_code::internal_error;
		if (retrieveCommand.FetchNext()) {
			return status_code::user_already_registered;
		}
		else {
			SACommand insertCommand(&this->connection_,"insert into Clients(nickname,password) values (:1,:2)");
			insertCommand << user.nickname.data() << user.password.data();
			insertCommand.Execute();
			if (!insertCommand.isExecuted())
				return status_code::internal_error;
			if (!insertCommand.RowsAffected())
				return status_code::internal_error;
			else
				return status_code::registered_succes;
		}
	}
	catch (SAException const& e)
	{
		std::cerr << e.ErrText().GetMultiByteChars() << std::endl;
		return status_code::internal_error;
	}
	catch (...)
	{
		std::cerr << "Exception occured in SQLiteDB::register_user method" << std::endl;
		return status_code::internal_error;
	}
}

SQLiteDB::status_code SQLiteDB::erase_user(user const& user) const noexcept
{
	try {
		if (!connection_.isConnected() || !connection_.isAlive())
			return status_code::internal_error;
		SACommand command(&this->connection_,"delete from Clients where Clients.nickname == (:1)");
		command << user.nickname.data();
		command.Execute();
		if (!command.isExecuted())
			return status_code::internal_error;
		if (!command.RowsAffected())
			return status_code::user_failed_to_remove;
		else
			return status_code::user_removed;
	}
	catch (SAException const& e)
	{
		std::cerr << e.ErrText().GetMultiByteChars() << std::endl;
		return status_code::internal_error;
	}
	catch (...)
	{
		std::cerr << "Exception occured in SQLiteDB::register_user method" << std::endl;
		return status_code::internal_error;
	}
}
