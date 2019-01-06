#pragma once


#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "async_windows_file_io.h"


struct AbstractFileDeliver
{
private:
	using ssl_sock = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
	typedef _STD unique_ptr<ssl_sock>	ssl_sock_ptr_t;
public:
	typedef _STD function<void()> CleanUpHandler;

	AbstractFileDeliver(HANDLE OpenedFileOnDisk, boost::asio::io_context& io_ctx, CleanUpHandler&& Handler)
		:	FileOnDisk(io_ctx, OpenedFileOnDisk),
			SocketPtr(nullptr),
			CleanUpHandler_(_STD move(Handler)),
			DoneFlag(false)
	{}
	AbstractFileDeliver(AbstractFileDeliver const&) = delete;
	AbstractFileDeliver& operator=(AbstractFileDeliver const&) = delete;

	virtual ~AbstractFileDeliver() {}

	virtual void			Start()								{	assert(SocketPtr != nullptr);	}
	BOOST_FORCEINLINE void	SetSocket(ssl_sock_ptr_t&& socket)	{	SocketPtr = _STD move(socket);	}
protected:
	enum { KB500 = 512000 };
	AsyncWindowsFileIO	FileOnDisk;
	ssl_sock_ptr_t		SocketPtr;
	char				Buffer[KB500];
	CleanUpHandler		CleanUpHandler_;
	bool				DoneFlag;
};

typedef _STD unique_ptr<AbstractFileDeliver>	AbstractFileDeliverPtr_t;