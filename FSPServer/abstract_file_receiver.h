#pragma once

#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "async_windows_file_io.h"
#include "protocol.h"

struct AbstractFileReceiver
{
private:
	using ssl_sock = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
	typedef _STD unique_ptr<ssl_sock>	ssl_sock_ptr_t;
public:
	typedef _STD function<void(bool)>	handler;

	AbstractFileReceiver(handler&& CompletionHandler, HANDLE OpenedFileOnDisk, uintmax_t FileSize, boost::asio::io_context& io_ctx)
		:	FileOnDisk(io_ctx, OpenedFileOnDisk),
			FileSize(FileSize),
			BytesWrottedToDisck(0),
			SocketPtr(nullptr),
			Handler(_STD move(CompletionHandler))		
	{}
	AbstractFileReceiver(AbstractFileReceiver const&) = delete;
	AbstractFileReceiver& operator=(AbstractFileReceiver const&) = delete;

	virtual ~AbstractFileReceiver() {}

	virtual void			Start()								{	assert(SocketPtr != nullptr);	}
	BOOST_FORCEINLINE void	SetSocket(ssl_sock_ptr_t&& socket)	{	SocketPtr = _STD move(socket);	}
	void					Close()
	{
		FileOnDisk.close();
		assert(SocketPtr != nullptr);
		boost::system::error_code ec;
		SocketPtr->shutdown(ec);
		SocketPtr->lowest_layer().close(ec);
	}
protected:
	BOOST_FORCEINLINE void	OnError() const		{	Handler(false);	}
	BOOST_FORCEINLINE void	OnSuccess() const	{	Handler(true);	}

	AsyncWindowsFileIO	FileOnDisk;
	uintmax_t			FileSize;
	uintmax_t			BytesWrottedToDisck;
	ssl_sock_ptr_t		SocketPtr;
	char				Buffer[fsp::protocol::MAX_COMPRESSED_SEGMENT_LENGTH];
	handler				Handler;
};

typedef _STD unique_ptr<AbstractFileReceiver>	AbstractFileReceiverPtr_t;