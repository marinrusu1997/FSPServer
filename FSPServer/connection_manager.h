#pragma once
#include <memory>
#include <map>
#include <mutex>

#include "backup_requests_manager.h"
#include "download_manager.h"
#include "fsp_ssl_versions.h"
#include "fsp_compressions.h"
#include "server_side_handler.h"

namespace fsp::protocol::message {
	struct message;
}

class connection;

class connection_manager final
{
public:
	typedef uint64_t								counter_t;
	struct DownloadQuerry
	{
		_STD string											QuerryID;
		_STD string											FileOwner;
		_STD string											FilePath;
		fsp::protocol::ssl_versions::version				SslVersion;
		_STD forward_list<fsp::protocol::compressions::compression> Compressions;
	};

	connection_manager(const connection_manager&) = delete;
	connection_manager& operator=(const connection_manager&) = delete;

	connection_manager();

	/// Add the specified connection to the manager and start it.
	void start(std::shared_ptr<connection>&& c);

	/// Notify that connection has done to write into TCP internall buffer
	void notify_shared_write_done(const counter_t BufferID);

	/// Notify clients about disconnect of specified client
	void notify_disconnection(std::shared_ptr<connection> const& Connection);

	/// Notify cliens about files of another user
	void notify_files_registration(std::shared_ptr<connection> const& Connection);

	/// Notify clients about addition of the file path from specified user
	void notify_file_add(const _STD string_view FilePath, _STD shared_ptr<connection> const& Connection);

	/// Notify clients about removing of file path from specified user
	void notify_path_removed(const _STD string_view FilePath, _STD shared_ptr<connection> const& Connection);

	/// Notify clients about renaming of file path from specified user
	void notify_path_renamed(const _STD string_view OldFilePath, const _STD string_view NewName, _STD shared_ptr<connection> const& Connection);

	/// Resolve download file querry
	void resolve_download_file_querry(DownloadQuerry&& DownloadQuerry, _STD shared_ptr<connection> const& Connection);

	/// Resolve download reply from file owner
	void resolve_download_reply(fsp::protocol::message::message& Reply);

	/// Stop all connections.
	void stop_all();

	/// Verify if client is not already logged
	bool isLogged(std::string_view nickname) noexcept;

	/// Return handler used to serve all connections
	auto& handler() { return handler_; }
private:
	typedef std::set<std::shared_ptr<connection>>	connections_set;
	///WARNING!! BE VERY CAREFULL WITH COUNTER FROM PAIR
	/// THIS WILL WORK ONLY IF ALL WRITE COMPLETION HANDLERS ARE EXECUTED ONE BY ONE IN THE SAME THREAD
	/// AND WE WILL NOT TOUCH COUNTER WHEN THEY ARE EXECUTED, 
	/// I.E WE MUST INITIALIZE IT BEFORE WE START IF WE RUN IN ANOTHER THREAD
	/// IF HANDLERS ARE DISPATCHED IN DIFERENT THREADS, THEN COUNTER MUST BE ATOMIC
	typedef _STD map<counter_t, _STD pair<_STD string, counter_t> > shared_buffers_t;

	void do_notify(const _STD string_view Notifier, _STD string&& Notification);

	friend fsp::protocol::compressions::compression GetCompression(connection_manager const& manager, 
		_STD forward_list<fsp::protocol::compressions::compression> const& querry);

	void	SetFileToCached(const _STD string_view FileOwner, const _STD string_view FilePath);
	bool	NotifyFileDownload(const _STD string_view FileOwner, const uintmax_t ID, fsp::protocol::compressions::compression Compression);
	void	DeliverFileFromLocalStorage(_STD shared_ptr<connection> const& Connection, compression SupportedCompression, 
		_STD string const& RequestID, _STD string const& LocalFilePath);

	/// Mutex for protecting maps
	std::mutex										mutex_;
	///Connections and their cliet models
	connections_set									connections_;
	/// Request handler
	fsp::impl::server::detail::server_side_handler	handler_;
	/// Storage location where shared buffers fro write are storred
	shared_buffers_t								shared_storage_;
	/// Count for connections that are in the processing state
	counter_t										processing_counter_;
	/// Storage for download querries
	requests_backup_manager							backup_requests_;
	/// Download manager
	download_manager								download_manager_;
	/// ID counter for shared storage
	static inline counter_t SharedStorageIDCounter			= 0;
	/// ID counter for download transactions
	static inline counter_t DownloadTransactionsIDCounter	= 1;
};

