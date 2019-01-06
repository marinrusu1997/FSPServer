#pragma once
#include <forward_list>
#include <thread>
#include <functional>
#include <mutex>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "fsp_ssl_versions.h"
#include "fsp_compressors.h"

#include "simple_timer.h"

#include "abstract_file_receiver.h"
#include "abstract_file_deliver.h"

#pragma comment (lib, "crypt32")

using fsp::protocol::ssl_versions::version;
using fsp::protocol::compressions::compression;
using namespace fsp::protocol::compressors;

namespace boost::asio::ssl {
	template <typename Stream>
	class stream;
}

class download_manager final
{
	struct TemporarySession;

	using ssl_sock = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
	typedef _STD unique_ptr<ssl_sock>	ssl_sock_ptr_t;
public:
	typedef _STD function<void()> TimerHandler_t;

	download_manager(uint16_t port);

	_NODISCARD const uint16_t								GetPort() const noexcept { return port_; }
	_NODISCARD const version								GetSslVersion() const noexcept { return version::sslv23; }
	_NODISCARD const _STD forward_list<_STD string_view>	GetSupportedCompressionsStr() const;
	_NODISCARD const _STD forward_list<compression>			GetSupportedCompressionsEnum() const;

	_NODISCARD const bool									BeginDownloadTransaction(uintmax_t ID,_STD string const& FileName, uintmax_t FileSize, compression Compression,
																		AbstractFileReceiver::handler&& DownloadCompletionHandler, TimerHandler_t && TimerHandler);
	_NODISCARD const bool									BeginUploadTransaction(const _STD string& FilePath, compression& SupportedCompression, uintmax_t& TransactionID);
private:
	_NODISCARD AbstractCompressor*							GetCompressor(compression c) { return fsp::protocol::compressors::GetCompressorPointer(c); }

	void													TryStartDownloadTransaction(const uintmax_t TransactionID, ssl_sock_ptr_t& TempSocketPtr) const noexcept;
	void													CloseDownloadTransaction(const uintmax_t TransactionID) const noexcept;
	_NODISCARD const size_t									EndDownloadTransaction(uintmax_t ID) noexcept;

	void													TryStartUploadTransaction(const uintmax_t TransactionID, ssl_sock_ptr_t& MovableSocketPtr) const noexcept;
	_NODISCARD const size_t									EndUploadTransaction(const uintmax_t TransactionID) noexcept;

	_STD string												GetPassword() const;
	void													StartAccept();
	void													HandleAccept(TemporarySession* new_session,	const boost::system::error_code& error);

	typedef _STD shared_ptr<simple_timer>			TimerPtr_t;
	typedef _STD map <uintmax_t, _STD pair<AbstractFileReceiverPtr_t,	TimerPtr_t>> DwnldTrMap_t;
	typedef _STD map <uintmax_t, _STD pair<AbstractFileDeliverPtr_t,	TimerPtr_t>> UpldTrMap_t;

	uint16_t						port_;
	boost::asio::io_context			io_context_;
	boost::asio::io_context::work	work_;
	boost::asio::ssl::context		ssl_context_;
	boost::asio::ip::tcp::acceptor	acceptor_;
	_STD thread						download_thread_;
	mutable _STD mutex				download_mutex_;
	DwnldTrMap_t					download_transactions_;
	mutable _STD mutex				upload_mutex_;
	UpldTrMap_t						upload_transactions_;

	static inline uintmax_t			upload_transactions_id_counter_ = 1;
};