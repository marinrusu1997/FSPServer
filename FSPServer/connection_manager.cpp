#include "connection_manager.h"
#include "connection.h"
#include "server_side_handler.h"

#include "fsp_commands.h"
#include "fsp_querry_types.h"
#include "fsp_notification_types.h"
#include "fsp_responses.h"
#include "message.h"

#include "Configuration.h"

#include <algorithm>
#include <iostream>


#define ENTER_CRITICAL_CODE_SECTION		{ std::lock_guard<std::mutex> lock(mutex_);
#define EXIT_CRITICAL_CODE_SECTION		}

#define BOOST_TIME_TO_WAIT_FOR_RESPONSE boost::posix_time::seconds(30)

connection_manager::connection_manager()
	: mutex_(),
	connections_(),
	handler_(),
	shared_storage_(),
	processing_counter_(0),
	backup_requests_(BOOST_TIME_TO_WAIT_FOR_RESPONSE),
	download_manager_((uint16_t)std::stoi(Configuration::get_instance().get_config_settings().try_get(
		persistency::keys::common::DOWNLOAD_PORT, { "8071" }).front()))
{}

void connection_manager::start(std::shared_ptr<connection>&& c)
{
	std::lock_guard<std::mutex> lock(mutex_);

	c->start();
	connections_.emplace(std::move(c));
}

/// TO DO SENT MESSAGE TO ANOUNCE TO DELETE FROM LIST
void connection_manager::notify_disconnection(std::shared_ptr<connection> const& Connection)
{
	using namespace fsp::impl::server::models;
	using namespace fsp::protocol::message;
	using namespace fsp::protocol::commands;
	namespace hdr = fsp::protocol::headers;

	// Get current state of the disconnecting client
	const auto CurrentStateOfStoppedConnection = Connection->client().state().load();
	// Snapshot of the nickname
	const _STD string NicknameOfStoppedConnection(Connection->client().nickname());
	// Make shared buffer with notification
	builders::StringNotificationMessage LogOutNotification;
	LogOutNotification
		.SetNotificationType(hdr::notifications::USER_LOGGED_OUT)
		.SetAditionalHeaders(hdr::FileOwner, Connection->client().nickname())
		.SetEndOfProtocolHeader();
	ENTER_CRITICAL_CODE_SECTION

		//std::cout << "NotifyDisconection before processing counter " << this->processing_counter_ << "\n";
		// Erase connection 
		connections_.erase(std::move(Connection));

	// If user was in processing state, it means that other users that are too in processing state knows about him
	// so they need to be notificated that user went of and remove him from their list of clients
	if (CurrentStateOfStoppedConnection == client::State::processing)
	{
		// FIRST OF ALL UPDATE PROCESSING COUNTER SO IT WOULD REFLECT REAL NUMBER OF PROCESSING CONNECTIONS
		// NEXT CHECK IF WE HAVE SOMEONE IN PROCESSING STATE,
		// BECAUSE ONLY THESE CONNECTIONS CAN REMOVE BUFFER FROM SHARED MEMORY POOL
		if (--this->processing_counter_ == 0)
			return;

		// NEXT STORE SHARED BUFFER WITH UPDATED VALUE OF THE SHARED PROCESSING COUNTER
		++SharedStorageIDCounter;
		shared_storage_.emplace(SharedStorageIDCounter, _STD pair(_STD move(LogOutNotification.build()), this->processing_counter_));
		const auto& SharedBufferView = shared_storage_[SharedStorageIDCounter].first;
		std::cout << "Stored " << SharedStorageIDCounter << "\n";

		// Notify other users that know about his one who left server
		// we dont need to check for nickname as this connection was already removed
		for (const auto& conn : connections_)
			if (conn->client().state().load() == client::State::processing)
				conn->start_shared_write(SharedBufferView, SharedStorageIDCounter);
	}
	_STD error_code ec;
	_STD filesystem::remove_all(NicknameOfStoppedConnection,ec);
	if (ec)
		std::cout << "Failed to remove user directory " << NicknameOfStoppedConnection << "\n";
	EXIT_CRITICAL_CODE_SECTION
}

void connection_manager::stop_all()
{
	ENTER_CRITICAL_CODE_SECTION
		handler_.stop();
	connections_.clear();
	processing_counter_ = 0;
	EXIT_CRITICAL_CODE_SECTION
}


auto connection_manager::isLogged(std::string_view nickname) noexcept -> bool
{
	ENTER_CRITICAL_CODE_SECTION
		auto iter = std::find_if(connections_.begin(), connections_.end(), [nickname](const auto& connection) {
		return (connection->client().nickname() == nickname) ? true : false;
			});
	return	iter != connections_.end();
	EXIT_CRITICAL_CODE_SECTION
}


void connection_manager::notify_files_registration(std::shared_ptr<connection> const& Connection)
{
	using namespace fsp::impl::server::models;
	using namespace fsp::protocol::message;
	using namespace fsp::protocol::commands;
	namespace hdr = fsp::protocol::headers;
	using namespace fsp::protocol::content_type_formats;

	// Getting nickname and files of our new user
	auto& Client = Connection->client();
	const auto	Nickname = Client.nickname();
	const auto& ClientVirtualFileSystem = Client.RemoteFileSystem();

	// Notification for other users
	builders::StringNotificationMessage UserRegisteredFilesNotification;
	UserRegisteredFilesNotification.SetNotificationType(hdr::notifications::USER_REGISTERED_FILES);
	UserRegisteredFilesNotification.SetAditionalHeaders(hdr::FileOwner, Nickname);
	if (!ClientVirtualFileSystem.Empty())
	{
		_STD forward_list<client::path_t> Paths;
		ClientVirtualFileSystem.Get(Paths);
		auto FilesBody = builders::build_body(Paths, content_type_format::CSV_LIST);
		UserRegisteredFilesNotification.SetAditionalHeaders(hdr::ContentLength, _STD to_string(FilesBody.size()));
		UserRegisteredFilesNotification.SetAditionalHeaders(hdr::ContentType, CSV_LIST);
		UserRegisteredFilesNotification.SetEndOfProtocolHeader();
		UserRegisteredFilesNotification.SetBody(FilesBody);
	}
	else
	{
		UserRegisteredFilesNotification.SetEndOfProtocolHeader();
	}

	auto& SharedBuffer = UserRegisteredFilesNotification.build();

	// Notification for our user
	builders::StringNotificationMessage FilesOfOtherUsersNotification;
	FilesOfOtherUsersNotification.SetNotificationType(hdr::notifications::FILES_OF_OTHER_USERS);

	ENTER_CRITICAL_CODE_SECTION

		// FIRST OF ALL CHECK IF WE HAVE SOMEONE IN PROCESSING STATE,
		// BECAUSE ONLY THESE CONNECTIONS CAN REMOVE BUFFER FROM SHARED MEMORY POOL
		// We check against 1, because if counter is 1, then only us are in connected state
		// so we have no clients to notify about our registration
		// But before return we will need to transition us to processing state
		// When this counter will be 2 or more then client will go to processing state after all connections are notified
		// this is done for optimization purposes in for loop
		// i.e we dont need to check against nickname
		if (++this->processing_counter_ == 1)
		{
			Connection->client().state().store(client::State::processing);
			return;
		}

	// Make key
	++SharedStorageIDCounter;
	// Store buffer, but with counter - 1, because our connection is counted aswell
	// and we dont want to be notified about our registration
	// furthermore, loop won't send message to us, we aren't in processing state atm
	// so counter will remain 1 after last write will be done => no one will be able to reclaim shared memory
	shared_storage_.emplace(SharedStorageIDCounter, _STD pair(_STD move(SharedBuffer), this->processing_counter_ - 1));
	const auto& SharedBufferView = shared_storage_[SharedStorageIDCounter].first;
	std::cout << "Stored " << SharedStorageIDCounter << "\n";

	_STD map<client::path_t, _STD forward_list<client::path_t>, _STD less<>> FileSystemsOfUsers;

	// Anounce another clients which are in processing state that we registered files of a new connected user
	// Check for nickname is not required, as our new connection is still in logged state at this momment
	for (auto& connection : connections_)
	{
		auto& CurrentClient = connection->client();
		if (CurrentClient.state().load() == client::State::processing)
		{
			CurrentClient.RemoteFileSystem().Get(FileSystemsOfUsers[CurrentClient.nickname().data()]);
			connection->start_shared_write(SharedBufferView, SharedStorageIDCounter);
		}
	}

	// Finishing up notification for our user with files of another users
	auto FilesOfUsersBody = builders::build_body(FileSystemsOfUsers);
	FilesOfOtherUsersNotification.SetAditionalHeaders(hdr::ContentLength, _STD to_string(FilesOfUsersBody.size()));
	FilesOfOtherUsersNotification.SetAditionalHeaders(hdr::ContentType, CSV_MAP);
	FilesOfOtherUsersNotification.SetEndOfProtocolHeader();
	FilesOfOtherUsersNotification.SetBody(FilesOfUsersBody);

	// Send notification to our client
	Connection->start_write(FilesOfOtherUsersNotification.build());

	// Atomicaly go to processing state
	Connection->client().state().store(client::State::processing);

	EXIT_CRITICAL_CODE_SECTION
}

void connection_manager::notify_shared_write_done(const connection_manager::counter_t BufferID)
{
	if (--this->shared_storage_[BufferID].second == 0)
	{
		std::cout << "Deleted " << BufferID << "\n";
		assert(this->shared_storage_.erase(BufferID) >= 1);
	}
}

void connection_manager::notify_file_add(const _STD string_view FileName, _STD shared_ptr<connection> const& Connection)
{
	namespace hdr = fsp::protocol::headers;
	using fsp::protocol::message::builders::StringNotificationMessage;

	const auto	NicknameOfNotifier = Connection->client().nickname();

	StringNotificationMessage AddNewFileNotification;
	AddNewFileNotification
		.SetNotificationType(hdr::notifications::USER_ADDED_NEW_FILE)
		.SetAditionalHeaders(hdr::FileOwner, NicknameOfNotifier)
		.SetAditionalHeaders(hdr::FileName, FileName)
		.SetEndOfProtocolHeader();

	ENTER_CRITICAL_CODE_SECTION
		do_notify(NicknameOfNotifier, _STD move(AddNewFileNotification.build()));
	EXIT_CRITICAL_CODE_SECTION
}

void connection_manager::notify_path_removed(const _STD string_view FilePath, _STD shared_ptr<connection> const& Connection)
{
	namespace hdr = fsp::protocol::headers;
	using fsp::protocol::message::builders::StringNotificationMessage;

	const auto	NicknameOfNotifier = Connection->client().nickname();

	StringNotificationMessage FileRemovedNotification;
	FileRemovedNotification
		.SetNotificationType(hdr::notifications::USER_DELETED_FILE)
		.SetAditionalHeaders(hdr::FileOwner, NicknameOfNotifier)
		.SetAditionalHeaders(hdr::FileName, FilePath)
		.SetEndOfProtocolHeader();

	ENTER_CRITICAL_CODE_SECTION
		do_notify(NicknameOfNotifier, _STD move(FileRemovedNotification.build()));
	EXIT_CRITICAL_CODE_SECTION
}

void connection_manager::notify_path_renamed(const _STD string_view OldFilePath, const _STD string_view NewName, _STD shared_ptr<connection> const& Connection)
{
	namespace hdr = fsp::protocol::headers;
	using fsp::protocol::message::builders::StringNotificationMessage;

	const auto	NicknameOfNotifier = Connection->client().nickname();

	StringNotificationMessage FileRenamedNotification;
	FileRenamedNotification
		.SetNotificationType(hdr::notifications::USER_RENAMED_FILE)
		.SetAditionalHeaders(hdr::FileOwner, NicknameOfNotifier)
		.SetAditionalHeaders(hdr::FileName, OldFilePath)
		.SetAditionalHeaders(hdr::FileNewName, NewName)
		.SetEndOfProtocolHeader();

	ENTER_CRITICAL_CODE_SECTION
		do_notify(NicknameOfNotifier, _STD move(FileRenamedNotification.build()));
	EXIT_CRITICAL_CODE_SECTION
}

void connection_manager::do_notify(const _STD string_view Notifier, _STD string&& Notification)
{
	using namespace fsp::impl::server::models;
	// FIRST OF ALL CHECK IF WE HAVE SOMEONE IN PROCESSING STATE,
	// BECAUSE ONLY THESE CONNECTIONS CAN REMOVE BUFFER FROM SHARED MEMORY POOL
	// We check against 1, because this call can be made only from a connection which is in processing state
	// so if he is the only connection we have no connections to anounce
	if (this->processing_counter_ == 1)
		return;

	// Make key
	++SharedStorageIDCounter;
	// We are counted aswell, this means that we have to anounce  (processing_counter_ - 1) connections
	shared_storage_.emplace(SharedStorageIDCounter, _STD pair(_STD move(Notification), this->processing_counter_ - 1));
	auto& SharedBufferView = shared_storage_[SharedStorageIDCounter].first;
	std::cout << "Stored " << SharedStorageIDCounter << "\n";

	// Go through client connections and notify them
	// We need to check against nickname, as our client is in processing state too and is in same collection with another connections
	for (const auto& connection : this->connections_)
		if (connection->client().state().load() == client::State::processing && connection->client().nickname() != Notifier)
			connection->start_shared_write(SharedBufferView, SharedStorageIDCounter);
}

fsp::protocol::compressions::compression GetCompression(connection_manager const& manager, 
	_STD forward_list<fsp::protocol::compressions::compression> const& Compressions)
{
	if (Compressions.empty())
		return fsp::protocol::compressions::compression::nocompression;

	const auto& ServerCompressions = manager.download_manager_.GetSupportedCompressionsEnum();
	for (const auto& ServerCompr : ServerCompressions)
		for (const auto& UserCompr : Compressions)
			if (ServerCompr == UserCompr)
				return ServerCompr;

	return fsp::protocol::compressions::compression::nocompression;
}

_NODISCARD _STD string CSVCompressions(_STD forward_list<_STD string_view> const& Compressions)
{
	_STD string csv;
	for (const auto& Compression : Compressions)
		csv.append(Compression).append(fsp::protocol::CSV_DELIMITER_STR);
	if (!csv.empty())
		csv.pop_back();
	return csv;
}

_NODISCARD BOOST_FORCEINLINE auto GetDownloadDenyHandler() noexcept
{
	namespace rsp = fsp::protocol::responses;
	using fsp::protocol::message::builders::StringReplyMessage;

	return [](auto&& c, auto compr, const auto& ReqID)
	{
		c->start_write(StringReplyMessage().SetReplyCode(rsp::DOWNLOAD_DENY).SetRequestID(ReqID).SetEndOfProtocolHeader().build());
	};
}

void	connection_manager::DeliverFileFromLocalStorage(_STD shared_ptr<connection> const& Connection, compression SupportedCompression, 
	_STD string const& RequestID, _STD string const& LocalFilePath)
{
	namespace hdr = fsp::protocol::headers;
	namespace rsp = fsp::protocol::responses;
	using fsp::protocol::message::builders::StringReplyMessage;

	uintmax_t TransactionID{ 0 };
	_STD error_code ec;
	auto FileSize = _STD filesystem::file_size(LocalFilePath, ec);

	if (download_manager_.BeginUploadTransaction(LocalFilePath, SupportedCompression, TransactionID) && !ec)
	{
		Connection->start_write(
			StringReplyMessage()
			.SetReplyCode(rsp::DOWNLOAD_FILE_QUERRY_APPROVED)
			.SetRequestID(RequestID)
			.SetAditionalHeaders(hdr::FileSize, _STD to_string(FileSize))
			.SetAditionalHeaders(hdr::DownloadPort, _STD to_string(download_manager_.GetPort()))
			.SetAditionalHeaders(hdr::DownloadTransactionID, _STD to_string(TransactionID))
			.SetAditionalHeaders(hdr::Compressions, fsp::protocol::compressions::Compression(SupportedCompression))
			.SetEndOfProtocolHeader()
			.build()
		);

		std::cout << "File will be delivered from local storage" << LocalFilePath << std::endl;
	}
	else
	{
		Connection->start_write(StringReplyMessage().SetReplyCode(rsp::DOWNLOAD_DENY).SetRequestID(RequestID).SetEndOfProtocolHeader().build());

		std::cout << "Failed to deliver file from local storage " << LocalFilePath << std::endl;
	}
}

void connection_manager::resolve_download_file_querry(DownloadQuerry&& DownloadQuerry, _STD shared_ptr<connection> const& Connection)
{
	namespace hdr = fsp::protocol::headers;
	namespace rsp = fsp::protocol::responses;
	using fsp::protocol::message::builders::StringReplyMessage;
	using fsp::protocol::message::builders::StringQuerryMessage;

	if (download_manager_.GetSslVersion() != DownloadQuerry.SslVersion)
	{
		Connection->start_write(StringReplyMessage().SetReplyCode(rsp::SSL_VERSION_INCOMPATIBLE).SetRequestID(DownloadQuerry.QuerryID).SetEndOfProtocolHeader().build());
		return;
	}

	_STD shared_ptr<connection> OwnerConnection{ nullptr };
	bool cached = false;

	ENTER_CRITICAL_CODE_SECTION
		if (auto&& iter = _STD find_if(connections_.begin(), connections_.end(), [&](const auto& c) {return c->client().nickname() == DownloadQuerry.FileOwner; });
			iter != connections_.end() && (*iter)->client().RemoteFileSystem().Find(DownloadQuerry.FilePath, cached))
	{
		OwnerConnection = *iter;
	}
	EXIT_CRITICAL_CODE_SECTION

	if (OwnerConnection == nullptr)
	{
		Connection->start_write(StringReplyMessage().SetReplyCode(rsp::DOWNLOAD_DENY).SetRequestID(DownloadQuerry.QuerryID).SetEndOfProtocolHeader().build());
		return;
	}

	auto SupportedCompression = GetCompression(*this, DownloadQuerry.Compressions);
	if (cached)
	{
		/// deliver async from local storage
		DeliverFileFromLocalStorage(Connection, SupportedCompression, DownloadQuerry.QuerryID,
			DownloadQuerry.FileOwner.append(DownloadQuerry.FilePath));
	}
	else
	{
		// if file was downloaded
			// make download transaction and begin it
			// send to client tranzaction id and he must came in 30 sec for it

		auto ReqIdBackUp = DownloadQuerry.QuerryID;
		

		if (!backup_requests_.IsDownloadPending(DownloadQuerry.FileOwner, DownloadQuerry.FilePath, Connection->weak_from_this(),
			SupportedCompression, _STD move(DownloadQuerry.QuerryID)))
		{
			++DownloadTransactionsIDCounter;
			StringQuerryMessage DownloadQuerryStr;
			DownloadQuerryStr.SetRequestID(_STD to_string(DownloadTransactionsIDCounter))
				.SetQuerryType(hdr::querries::DOWNLOAD_FILE)
				.SetAditionalHeaders(hdr::FileName, DownloadQuerry.FilePath)
				.SetAditionalHeaders(hdr::SslVersion, fsp::protocol::ssl_versions::Version(download_manager_.GetSslVersion()))
				.SetAditionalHeaders(hdr::Compressions, CSVCompressions(download_manager_.GetSupportedCompressionsStr()))
				.SetEndOfProtocolHeader();
			OwnerConnection->start_write(DownloadQuerryStr.build());

			requests_backup_manager::DownloadInfo info(_STD move(DownloadQuerry.FileOwner), _STD move(DownloadQuerry.FilePath));

			info.AddRequest(Connection->weak_from_this(), SupportedCompression, _STD move(ReqIdBackUp));

			backup_requests_.BackupAndStartTimer(
				DownloadTransactionsIDCounter, 
				_STD move(info), 
				Connection->io_context(),
				GetDownloadDenyHandler()
			);

			std::cout << "Request for file download was sent with ID " << DownloadTransactionsIDCounter << std::endl;
		}
	}
}

void	connection_manager::SetFileToCached(const _STD string_view FileOwner, const _STD string_view FilePath)
{
	ENTER_CRITICAL_CODE_SECTION
		if (auto&& iter = _STD find_if(connections_.begin(), connections_.end(), [&](const auto& c) {return c->client().nickname() == FileOwner; });
			iter != connections_.end())
	{
		assert((*iter)->client().RemoteFileSystem().SetCached(FilePath, true));
	}
	EXIT_CRITICAL_CODE_SECTION
}

bool	connection_manager::NotifyFileDownload(const _STD string_view FileOwner, const uintmax_t ID, fsp::protocol::compressions::compression Compression)
{
	using fsp::protocol::message::builders::StringNotificationMessage;
	using namespace fsp::protocol::headers::notifications;
	using namespace fsp::protocol::headers;
	StringNotificationMessage DownloadNotification;
	DownloadNotification
		.SetNotificationType(USER_REQUESTED_FILE_DOWNLOAD)
		.SetAditionalHeaders(DownloadTransactionID, _STD to_string(ID))
		.SetAditionalHeaders(DownloadPort, _STD to_string(download_manager_.GetPort()))
		.SetAditionalHeaders(Compressions, fsp::protocol::compressions::Compression(Compression))
		.SetEndOfProtocolHeader();

	ENTER_CRITICAL_CODE_SECTION
		if (auto&& iter = _STD find_if(connections_.begin(), connections_.end(), [&](const auto& c) {return c->client().nickname() == FileOwner; });
			iter != connections_.end())
	{
		(*iter)->start_write(DownloadNotification.build());
		return true;
	}
	EXIT_CRITICAL_CODE_SECTION

		return false;
}

void connection_manager::resolve_download_reply(fsp::protocol::message::message& Reply)
{
	namespace hdr = fsp::protocol::headers;
	namespace rsp = fsp::protocol::responses;
	using fsp::protocol::message::builders::StringReplyMessage;
	
	std::cout << "Resolve download querry " << std::endl;

	auto RequestID		= _STD stoull(Reply[hdr::CommandId]);
	auto Compression	= GetCompression(*this, fsp::protocol::compressions::CSVCompressions(Reply[hdr::Compressions]));
	_STD string_view FilePath;
	_STD string_view FileOwner;
	
	/// We do a trick there, if request is valid, but reply is Deny
	/// we just won't stop timer, so he will do cleaning job for us
	/// we handle oly good case, worst case is handled by timer
	if (Reply[hdr::ReplyCode] == rsp::DOWNLOAD_FILE_QUERRY_APPROVED && backup_requests_.IsRequestIDValid(RequestID, FilePath, FileOwner))
	{
		/// Get local path
		_STD string LocalFilePath(FileOwner); LocalFilePath.append(FilePath);
		/// Begin download transaction
		const auto BeginTransactionStatus = this->download_manager_.BeginDownloadTransaction(RequestID,LocalFilePath,_STD stoull(Reply[hdr::FileSize]), Compression,
			[this, RequestID = RequestID, FilePath = _STD string(FilePath), FileOwner = _STD string(FileOwner)](bool Succes) 
		{
			if (Succes)
			{
				this->SetFileToCached(FileOwner, FilePath);
				const_cast<_STD remove_const<_STD remove_reference<decltype(FileOwner)>::type>::type&>(FileOwner).append(FilePath);
				this->backup_requests_.ForEachAndRemoveWrapper(RequestID, [&](auto&& SharedConnection, auto compr, const auto& ReqID)
					{
						this->DeliverFileFromLocalStorage(SharedConnection, compr, ReqID, FileOwner);
					});
			}
			else
			{
				this->backup_requests_.ForEachAndRemoveWrapper(RequestID, 
					GetDownloadDenyHandler());
			}
		},
		[this,RequestID = RequestID]() 
		{
			this->backup_requests_.ForEachAndRemoveWrapper(RequestID, 
				GetDownloadDenyHandler());
			}
		);

		std::cout << "Download Transaction Stored with ID" << RequestID << std::endl;

		/// Even if notification fails, i.e maybe user logged out, we dont have to do clean
		/// Transaction already started, so when timer will expire, he will clear all clients atomically, and will remove transaction
		if (BeginTransactionStatus == true) {
			NotifyFileDownload(FileOwner, RequestID, Compression);
			std::cout << FileOwner << " notification sent" << std::endl;
		}
		else { /// If transaction failed, we have to manually notify clients who are w8 for downloading, because we stopped timer earlier
			this->backup_requests_.ForEachAndRemoveWrapper(RequestID, 
				GetDownloadDenyHandler());
		}	
	}
}