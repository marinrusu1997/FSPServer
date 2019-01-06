#include "Configuration.h"

Configuration::Configuration() : 
	users_db_ptr_(nullptr)
{}

Configuration&	Configuration::get_instance() noexcept {
	static Configuration configuration;
	return configuration;
}

void Configuration::set_users_db(std::unique_ptr<IUsersDB> udb_ptr) noexcept {
	this->users_db_ptr_.reset(udb_ptr.release());
}

IUsersDB* Configuration::get_users_db() const noexcept {
	return this->users_db_ptr_.get();
}
