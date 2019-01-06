#include "download_manager.h"
#include <algorithm>
#include <iterator>
#include <istream>
#include <filesystem>
#include <future>
#include <chrono>

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/streambuf.hpp>

#include <AtlBase.h>
#include <atlconv.h>


#include "protocol.h"

#include "compressed_file_receiver.h"
#include "stream_mode_file_receiver.h"

#include "stream_file_deliver.h"
#include "compressed_file_deliver.h"

#include <iostream>

#define PATH_TO_CERTIFICATE_CHAIN_FILE	"C:\\Users\\dimar\\source\\repos\\FSP\\FSPServer\\FSPServer\\Self Signed Certificates\\fsp_server.crt"
#define PATH_TO_PRIVATE_KEY_FILE		"C:\\Users\\dimar\\source\\repos\\FSP\\FSPServer\\FSPServer\\Self Signed Certificates\\fsp_server.key"
#define PATH_TO_TMP_DH_FILE				"C:\\Users\\dimar\\source\\repos\\FSP\\FSPServer\\FSPServer\\Self Signed Certificates\\dh1024.pem"

#define BOOST_TIME_TO_WAIT_BEFORE_TRANSACTION_EXPIRES boost::posix_time::seconds(30)

#define ENTER_CRITICAL_SECTION(Mutex)	{ \
											_STD lock_guard<_STD mutex> guard((Mutex));
#define	EXIT_CRITICAL_SECTION			}

#define _BOOST	::boost:: 
#define _ASIO	_BOOST asio:: 
#define _FSP	::fsp::protocol:: 

///    TEMP SESSION  ---------------------------------------------------------
struct download_manager::TemporarySession
{
	TemporarySession(boost::asio::io_context& io_ctx, boost::asio::ssl::context& ssl_ctx,
		download_manager& my_manager);

	ssl_sock::lowest_layer_type& socket()
	{
		return TempSocketPtr_->lowest_layer();
	}

	void start();

private:
	void	HandleHandshake(const boost::system::error_code& error);

	struct HelloMessage
	{
		_STD string HelloType;
		uintmax_t	TransactionID;
	};

	_NODISCARD const bool	TryInitTransaction(_STD chrono::seconds Duration,HelloMessage& InitHello);

	download_manager&	MyManager_;
	ssl_sock_ptr_t		TempSocketPtr_;
};


download_manager::TemporarySession::TemporarySession(boost::asio::io_context& io_ctx, boost::asio::ssl::context& ssl_ctx,
	download_manager& my_manager)
	: MyManager_(my_manager),
	TempSocketPtr_(new ssl_sock(io_ctx, ssl_ctx))
{}

void download_manager::TemporarySession::start()
{
	TempSocketPtr_->async_handshake(boost::asio::ssl::stream_base::server,
		boost::bind(&TemporarySession::HandleHandshake, this,
			boost::asio::placeholders::error));
}

_NODISCARD const bool	download_manager::TemporarySession::TryInitTransaction(_STD chrono::seconds Duration, HelloMessage& HelloMessage)
{
	_ASIO streambuf TempBuffer;

	auto status = _STD async(_STD launch::async, [&]() { _ASIO read_until(*TempSocketPtr_, TempBuffer, _FSP LINE_SEPARATOR); })
		.wait_for(Duration);

	switch (status)
	{
	case std::future_status::deferred:
		//... should never happen with std::launch::async
		goto failure;
	case std::future_status::ready:
	{
		try{
			_STD istream is(&TempBuffer);
			_STD string Message;
			_STD getline(is, Message);

			_STD vector<_STD string> tokens;
			_BOOST split(tokens, Message, [](const auto chr) {return chr == _FSP DOWNLOAD_TRANSACTION_DELIM_CHR; });

			if (tokens.size() != 2)
				goto failure;
	
			HelloMessage.HelloType		= _STD move(tokens[_FSP SENDER_RECEIVER_POSITION_DWNLD]);
			HelloMessage.TransactionID	= _STD stoull(tokens[_FSP DOWNLOAD_TRANSACTION_ID_POS]);
			return true;
		}
		catch (...) {
			return false;
		}	
	}
		break;
	case std::future_status::timeout:
		goto failure;
	}
failure:
	return false;
}

void download_manager::TemporarySession::HandleHandshake(const boost::system::error_code& error)
{
	if (!error)
	{
		HelloMessage HelloMsg;
		if (!TryInitTransaction(_STD chrono::seconds{ 60 }, HelloMsg))
			goto cleanup;

		if (HelloMsg.HelloType == _FSP RECEIVER_CODE_STR)
		{
			MyManager_.TryStartUploadTransaction(HelloMsg.TransactionID, TempSocketPtr_);
			goto cleanup;
		}

		if (HelloMsg.HelloType == _FSP SENDER_CODE_STR)
		{
			MyManager_.TryStartDownloadTransaction(HelloMsg.TransactionID, TempSocketPtr_);
			goto cleanup;
		}
	}
cleanup:
	delete this;
}

///    DOWNLOAD MANAGER  -----------------------------------------------------
download_manager::download_manager(uint16_t port)
	: port_(port),
	io_context_(),
	work_(io_context_),
	ssl_context_(boost::asio::ssl::context::sslv23),
	acceptor_(io_context_, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
	download_thread_([this]() {io_context_.run(); })
{
	ssl_context_.set_options(
		boost::asio::ssl::context::default_workarounds
		| boost::asio::ssl::context::no_sslv2
		| boost::asio::ssl::context::single_dh_use);
	ssl_context_.set_password_callback(boost::bind(&download_manager::GetPassword, this));
	ssl_context_.use_certificate_chain_file(PATH_TO_CERTIFICATE_CHAIN_FILE);
	ssl_context_.use_private_key_file(PATH_TO_PRIVATE_KEY_FILE, boost::asio::ssl::context::pem);
	ssl_context_.use_tmp_dh_file(PATH_TO_TMP_DH_FILE);

	StartAccept();
}

_STD string download_manager::GetPassword() const
{
	return "FSP";
}

void download_manager::StartAccept()
{
	auto new_session = new TemporarySession(io_context_, ssl_context_, *this);
	acceptor_.async_accept(new_session->socket(),
		boost::bind(&download_manager::HandleAccept, this, new_session,
			boost::asio::placeholders::error));
}

void download_manager::HandleAccept(TemporarySession* new_session,
	const boost::system::error_code& error)
{
	if (!error)
	{
		new_session->start();
	}
	else
	{
		delete new_session;
	}

	StartAccept();
}

_NODISCARD const _STD forward_list<_STD string_view> download_manager::GetSupportedCompressionsStr() const
{
	// Actually we have support only for zlib and gzip
	// Need to install libraries for lzma and bzip2
	/*_STD forward_list<_STD string_view> Compressions;
	_STD transform(
		_STD begin(compressors),
		_STD end(compressors),
		_STD front_inserter(Compressions),
		[](const auto& pair) {return fsp::protocol::compressions::Compression(pair.first); }
	);
	return Compressions;*/

	return { fsp::protocol::compressions::ZLIB, fsp::protocol::compressions::GZIP };
}

_NODISCARD const _STD forward_list<compression>			download_manager::GetSupportedCompressionsEnum() const
{
	//_STD forward_list<compression> Compressions;
	//_STD transform(
	//	_STD begin(compressors),
	//	_STD end(compressors),
	//	_STD front_inserter(Compressions),
	//	[](const auto& pair) {return pair.first; }
	//);
	//return Compressions;
	return { fsp::protocol::compressions::compression::zlib, fsp::protocol::compressions::compression::gzip };
}

_NODISCARD const bool									download_manager::BeginDownloadTransaction(uintmax_t ID, _STD string const& FileName, uintmax_t FileSize, compression Compression,
	AbstractFileReceiver::handler&& DownloadCompletionHandler, TimerHandler_t && TimerHandler)
{
	_STD filesystem::create_directories(_STD filesystem::path(FileName).parent_path());

	HANDLE LocalFileHandle = CreateFile(
		CA2W(FileName.c_str()),
		GENERIC_WRITE,
		FILE_SHARE_DELETE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL
	);

	if (LocalFileHandle == INVALID_HANDLE_VALUE)
		return false;

	AbstractFileReceiverPtr_t FileReceiver(nullptr);

	auto InternalDownloadCompletionHandlerWrapper = [this, ID = ID, DownloadCompletionHandler = _STD move(DownloadCompletionHandler)](const bool Success)
	{
		this->CloseDownloadTransaction(ID);	// this will close handle, so next upload transactions can take place	
		DownloadCompletionHandler(Success);	
		assert(this->EndDownloadTransaction(ID)); 		
	};

	if (Compression == fsp::protocol::compressions::compression::nocompression)
	{
		FileReceiver = _STD make_unique<StreamModeFileReceiver>
			(_STD move(InternalDownloadCompletionHandlerWrapper), LocalFileHandle,
				FileSize, this->io_context_
				);
	}
	if (Compression == fsp::protocol::compressions::compression::zlib)
	{
		FileReceiver = _STD make_unique<CompressedModeFileReceiver>
			(_STD move(InternalDownloadCompletionHandlerWrapper), GetCompressor(Compression),
				LocalFileHandle, FileSize, this->io_context_
				);
	}

	auto timer = _STD make_shared <simple_timer>(io_context_);
	timer->set_handler([this, ID = ID, TimerHandler = _STD move(TimerHandler), self = timer->shared_from_this()](const auto& error_code)
	{
		if (error_code != boost::asio::error::operation_aborted && *self)
		{
			TimerHandler();
			assert(this->EndDownloadTransaction(ID));
			std::cout << "Download timer expired, transaction was removed " << ID << std::endl;
		}
		std::cout << "download timer aborted, transaction was not removed " << ID << std::endl;
		const_cast<TimerPtr_t&>(self).reset();
	}
	);

	ENTER_CRITICAL_SECTION(download_mutex_)
		this->download_transactions_.emplace(ID, _STD pair{ _STD move(FileReceiver), _STD move(timer) });
		this->download_transactions_[ID].second->start(BOOST_TIME_TO_WAIT_BEFORE_TRANSACTION_EXPIRES);
		return true;
	EXIT_CRITICAL_SECTION
}

void					download_manager::CloseDownloadTransaction(const uintmax_t TransactionID) const noexcept
{
	ENTER_CRITICAL_SECTION(download_mutex_)
		if (auto&& iter = this->download_transactions_.find(TransactionID); iter != this->download_transactions_.end())
		{
			iter->second.first->Close();
		}
	EXIT_CRITICAL_SECTION
}

_NODISCARD const size_t	download_manager::EndDownloadTransaction(uintmax_t ID) noexcept
{
	ENTER_CRITICAL_SECTION(download_mutex_)
		return this->download_transactions_.erase(ID);
	EXIT_CRITICAL_SECTION
}

void	download_manager::TryStartDownloadTransaction(const uintmax_t TransactionID, ssl_sock_ptr_t& TempSocketPtr) const noexcept
{
	ENTER_CRITICAL_SECTION(download_mutex_)
		if (auto&& iter = this->download_transactions_.find(TransactionID); iter != this->download_transactions_.end())
		{
			iter->second.second->stop();
			assert(TempSocketPtr->write_some(_ASIO buffer(_STD string_view(_FSP START_TRANSMISSION_BYTE))) == 1);
			iter->second.first->SetSocket(_STD move(TempSocketPtr));
			assert(TempSocketPtr == nullptr);
			iter->second.first->Start();
		}
	EXIT_CRITICAL_SECTION
}

_NODISCARD const bool	download_manager::BeginUploadTransaction(const _STD string& FilePath, compression& SupportedCompression, uintmax_t& TransactionID)
{
	_STD error_code ec;
	const auto FileSize = _STD filesystem::file_size(FilePath, ec);
	if (FileSize == 0 || ec)
		return false;

	HANDLE LocalFileHandle = CreateFile(CA2W(FilePath.c_str()),
		GENERIC_READ,
		FILE_SHARE_DELETE | FILE_SHARE_READ, //share read with anothread upload transactions and remove for updates from client, i.e delete or modified
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL);

	if (LocalFileHandle == INVALID_HANDLE_VALUE)
		return false;

	if (FileSize > 104857600) // 100 MB
		SupportedCompression = SupportedCompression == compression::zlib ? SupportedCompression : compression::nocompression;
	else
		SupportedCompression = compression::nocompression;

	AbstractFileDeliverPtr_t FileDeliver;
	auto CleanUpHandler = [this, ID = upload_transactions_id_counter_]() {	assert(this->EndUploadTransaction(ID)); };
	if (SupportedCompression == compression::nocompression)
		FileDeliver = _STD make_unique<StreamModeFileDeliver>(LocalFileHandle, this->io_context_, _STD move(CleanUpHandler));
	if (SupportedCompression == compression::zlib)
		FileDeliver = _STD make_unique<CompressedModeFileDeliver>(LocalFileHandle, this->io_context_, _STD move(CleanUpHandler), GetCompressor(SupportedCompression));

	auto timer = _STD make_shared <simple_timer>(io_context_);
	timer->set_handler([this, ID = upload_transactions_id_counter_, self = timer->shared_from_this()](const auto& error_code)
	{
		if (error_code != boost::asio::error::operation_aborted && *self)
		{
			assert(this->EndUploadTransaction(ID));
			std::cout << "Upload timer expired, transaction was removed " << ID << std::endl;
		}
		std::cout << "upload timer aborted, transaction was not removed " << ID << std::endl;
		const_cast<TimerPtr_t&>(self).reset();
	}
	);

	TransactionID = upload_transactions_id_counter_;

	ENTER_CRITICAL_SECTION(upload_mutex_)
		this->upload_transactions_.emplace(upload_transactions_id_counter_, _STD pair{ _STD move(FileDeliver), _STD move(timer) });
		this->upload_transactions_id_counter_++;
		return true;
	EXIT_CRITICAL_SECTION
}

_NODISCARD const size_t		download_manager::EndUploadTransaction(const uintmax_t TransactionID) noexcept
{
	ENTER_CRITICAL_SECTION(upload_mutex_)
		return this->upload_transactions_.erase(TransactionID);
	EXIT_CRITICAL_SECTION
}

void						download_manager::TryStartUploadTransaction(const uintmax_t TransactionID, ssl_sock_ptr_t& TempSocketPtr) const noexcept
{
	ENTER_CRITICAL_SECTION(upload_mutex_)
		if (auto&& iter = upload_transactions_.find(TransactionID); iter != upload_transactions_.end())
		{
			iter->second.second->stop();
			iter->second.first->SetSocket(_STD move(TempSocketPtr));
			assert(TempSocketPtr == nullptr);
			iter->second.first->Start();
		}
	EXIT_CRITICAL_SECTION
}