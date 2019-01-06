#include "request_scheduler.h"
#include "server_side_handler.h"
#include "protocol.h"
#include "fsp_responses.h"
#include "fsp_commands.h"
#include "fsp_querry_types.h"
#include "fsp_ssl_versions.h"
#include "fsp_compressions.h"
#include "Configuration.h"
#include "connection_manager.h"
#include "connection.h"
#include "IUsersDB.h"


namespace hdr = fsp::protocol::headers;
namespace rsp = fsp::protocol::responses;
namespace msg = fsp::protocol::message;
namespace cmd = fsp::protocol::commands;
namespace bld = fsp::protocol::message::builders;

using namespace fsp::impl::server::detail;


void server_side_handler::stop()
{
	request_scheduler::get_instance().stop_threads();
}


void server_side_handler::handle_message(fsp::protocol::message::message& Request, std::shared_ptr<connection>&& connection)
{
	const auto& Command		= Request.command();
	const auto& CommandId	= Request[hdr::CommandId];


	if (Command == cmd::SIGNIN)
	{
		request_scheduler::get_instance().post( scheduler_priority::high,
			[this, Connection = connection->weak_from_this(), CommandId = CommandId, Nickname = Request[hdr::Nickname], Password = Request[hdr::Password]]
		()
		{
			this->do_signin(CommandId, Nickname, Password, Connection);
		});

		return;
	}

	if (Command == cmd::SIGNUP)
	{
		request_scheduler::get_instance().post( scheduler_priority::high,
			[this, Connection = connection->weak_from_this(), CommandId = CommandId, Nickname = Request[hdr::Nickname], Password = Request[hdr::Password]]
		()
		{
			this->do_signup(CommandId, Nickname, Password, Connection);
		});

		return;
	}

	if (Command == cmd::PUSH_FILES)
	{
		auto files = _STD make_unique<_STD forward_list<_STD string> >();
		Request.body(*files);

		request_scheduler::get_instance().post( scheduler_priority::medium,
			[this, Connection = connection->weak_from_this(), CommandId = CommandId, Files = std::move(files)]
		()
		{
			this->do_push_files(CommandId, std::move(const_cast<_STD unique_ptr<_STD forward_list<_STD string>>&>(Files)), Connection);
		});

		return;
	}

	if (Command == cmd::QUERRY)
	{
		resolve_querry(Request, connection);
		return;
	}

	if (Command == cmd::DELETE_ACCOUNT)
	{
		request_scheduler::get_instance().post( scheduler_priority::low,
			[this, Connection = connection->weak_from_this(), CommandId = CommandId]
		()
		{
			this->do_delete_account(CommandId, Connection);
		});

		return;
	}

	if (Command == cmd::LOGOUT)
	{
		request_scheduler::get_instance().post( scheduler_priority::very_high,
			[this, Connection = connection->weak_from_this()]
		()
		{
			this->do_logout(Connection);
		});

		return;
	}

}

void server_side_handler::resolve_querry(fsp::protocol::message::message& Request, std::shared_ptr<connection>& Connection)
{
	const auto& QuerryType = Request[fsp::protocol::headers::QuerryType];

	if (QuerryType == fsp::protocol::headers::querries::DOWNLOAD_FILE)
	{
		request_scheduler::get_instance().post(scheduler_priority::medium,
			[this, FilePath = _STD move(Request[hdr::FileName]), FileOwner = _STD move(Request[hdr::FileOwner]),
			RequestID = _STD move(Request[hdr::CommandId]), SslVersion = _STD move(Request[hdr::SslVersion]),
			Compressions = _STD move(Request[hdr::Compressions]), Connection = Connection->weak_from_this()]
		()
		{
			this->do_download_file(
				{
				_STD move(const_cast<decltype(RequestID)&>(RequestID)),
				_STD move(const_cast<decltype(FileOwner)&>(FileOwner)),
				_STD move(const_cast<decltype(FilePath)&>(FilePath)),
				_STD move(const_cast<decltype(SslVersion)&>(SslVersion)),
				_STD move(const_cast<decltype(Compressions)&>(Compressions))
				},
				Connection);
		});
	}

	if (QuerryType == fsp::protocol::headers::querries::ADD_FILE)
	{
		request_scheduler::get_instance().post(scheduler_priority::medium,
			[this, FileName = _STD move(Request[hdr::FileName]), RequestID = _STD move(Request[hdr::CommandId]), Connection = Connection->weak_from_this()]
		()
		{
			this->do_add_file(RequestID,FileName, Connection);
		});
	}
	if (QuerryType == hdr::querries::REMOVE_PATH)
	{
		request_scheduler::get_instance().post(scheduler_priority::high,
			[this, FileName = _STD move(Request[hdr::FileName]), RequestID = _STD move(Request[hdr::CommandId]), Connection = Connection->weak_from_this()]
		()
		{
			this->do_remove_path(RequestID, FileName, Connection);
		});
	}
	if (QuerryType == hdr::querries::RENAME_PATH)
	{
		request_scheduler::get_instance().post(scheduler_priority::high,
			[this, OldFilePath = _STD move(Request[hdr::FileName]), NewName = _STD move(Request[hdr::FileNewName]),
			RequestID = _STD move(Request[hdr::CommandId]), Connection = Connection->weak_from_this()]
		()
		{
			this->do_remame_path(RequestID, OldFilePath, NewName, Connection);
		});
	}
}

void server_side_handler::do_signin(const std::string_view RequestID, const std::string_view login, const std::string_view password, std::weak_ptr<connection> const& connection_weak)
{
	if (connection_weak.expired())
		return;
	auto connection = connection_weak.lock();

	switch (Configuration::get_instance().get_users_db()->retrieve_user({ login, password }))
	{
	case IUsersDB::status_code::user_credentials_ok:
	{
		if (connection->manager().isLogged(login))
			connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::USER_ALREADY_LOGGED).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
		else {
			connection->client().nickname(login);
			connection->client().state().store(fsp::impl::server::models::client::State::logged);
			connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::LOGIN_SUCCESSFULL).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
		}
	}
	break;
	case IUsersDB::status_code::user_not_found:
	{
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::NICKNAME_INCORRECT).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	break;
	case IUsersDB::status_code::user_pswd_incorrect:
	{
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::PASSWORD_INCORRECT).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	break;
	case IUsersDB::status_code::internal_error:
	{
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::INTERNAL_SERVER_ERR).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	break;
	}
}

void server_side_handler::do_signup(const std::string_view RequestID, const std::string_view login, const std::string_view password, std::weak_ptr<connection> const& connection_weak)
{
	if (connection_weak.expired())
		return;
	auto connection = connection_weak.lock();

	switch (Configuration::get_instance().get_users_db()->register_user({ login, password }))
	{
	case IUsersDB::status_code::registered_succes:
	{
		connection->client().nickname(login);
		connection->client().state().store(fsp::impl::server::models::client::State::logged);
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::REGISTERING_SUCCESSFULL).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	break;
	case IUsersDB::status_code::user_already_registered:
	{
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::USER_ALREADY_REGISTERED).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	break;
	case IUsersDB::status_code::internal_error:
	{
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::INTERNAL_SERVER_ERR).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	break;
	}
}

void server_side_handler::do_delete_account(const std::string_view RequestID, std::weak_ptr<connection> const& connection_weak)
{
	if (connection_weak.expired())
		return;
	auto connection = connection_weak.lock();
	switch (Configuration::get_instance().get_users_db()->erase_user({connection->client().nickname(),""}))
	{
	case IUsersDB::status_code::user_removed:
	{
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::DELETING_ACCOUNT_SUCCESSFULL).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	break;
	case IUsersDB::status_code::user_failed_to_remove:
	{
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::REMOVE_ACCOUNT_FAILED).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	break;
	case IUsersDB::status_code::internal_error:
	{
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::INTERNAL_SERVER_ERR).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	}
}

void server_side_handler::do_logout(std::weak_ptr<connection> const& connection_weak)
{
	if (connection_weak.expired())
		return;
	connection_weak.lock()->stop();
}

void server_side_handler::do_push_files(const std::string_view RequestID, const _STD unique_ptr<_STD forward_list<_STD string>> PathsList, std::weak_ptr<connection> const& connection_weak)
{
	if (connection_weak.expired())
		return;
	auto connection = connection_weak.lock();

	try {
		auto& RemoteFileSystem = connection->client().RemoteFileSystem();
		for (const auto& Path : *PathsList)
			RemoteFileSystem.Add(Path);
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::PUSH_FILES_SUCCESS).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	catch (...) {
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::INTERNAL_SERVER_ERR).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
		return;
	}

	connection->manager().notify_files_registration(connection);
}

void server_side_handler::do_add_file(const std::string_view RequestID,const _STD string_view FileName, _STD weak_ptr<connection> const& connection_weak)
{
	if (connection_weak.expired())
		return;
	auto connection = connection_weak.lock();

	try {
		connection->client().RemoteFileSystem().Add(FileName);
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::ADD_FILE_SUCCESSFULL).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
	}
	catch (...) {
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::INTERNAL_SERVER_ERR).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
		return;
	}
	
	connection->manager().notify_file_add(FileName, connection);
}

void server_side_handler::do_remove_path(const std::string_view RequestID, const _STD string_view PathName, _STD weak_ptr<connection> const& connection_weak)
{
	if (connection_weak.expired())
		return;
	auto connection = connection_weak.lock();

	try {
		if (connection->client().RemoteFileSystem().Remove(PathName))
			connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::REMOVE_FILE_SUCCESSFULL).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
		else {
			connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::REMOVE_FILE_FAILED).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
			return;
		}
	}
	catch (...) {
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::INTERNAL_SERVER_ERR).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
		return;
	}

	connection->manager().notify_path_removed(PathName, connection);
}

void server_side_handler::do_remame_path(const _STD string_view RequestID, const _STD string_view OldFilePath, const _STD string_view NewName, _STD weak_ptr<connection> const& connection_weak)
{
	if (connection_weak.expired())
		return;
	auto connection = connection_weak.lock();

	try {
		if (connection->client().RemoteFileSystem().Rename(OldFilePath,NewName))
			connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::RENAME_FILE_SUCCESSFULL).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
		else {
			connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::RENAME_FILE_FAILED).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
			return;
		}
	}
	catch (...) {
		connection->start_write(bld::StringReplyMessage().SetReplyCode(rsp::INTERNAL_SERVER_ERR).SetRequestID(RequestID).SetEndOfProtocolHeader().build());
		return;
	}

	connection->manager().notify_path_renamed(OldFilePath, NewName, connection);
}

void server_side_handler::do_download_file(DownloadInfo&& downloadInfo, _STD weak_ptr<connection> const& connection_weak)
{
	if (connection_weak.expired())
		return;
	auto connection = connection_weak.lock();

	connection->manager().resolve_download_file_querry(
		{ 
		_STD move(downloadInfo.RequestID),
		_STD move(downloadInfo.FileOwner),
		_STD move(downloadInfo.FilePath),
		fsp::protocol::ssl_versions::Version(downloadInfo.SslVersion),
		CSVCompressions(downloadInfo.Compressions)
		}, 
		connection);
}