#include "stream_mode_file_receiver.h"
#include <boost/bind.hpp>

#include <iostream>

#define _ASIO	::boost::asio::
#define _BOOST	::boost::

void StreamModeFileReceiver::Start()
{
	_STD cout << "start stream mode" << _STD endl;
	_ASIO async_read(*SocketPtr, _ASIO buffer(Buffer), _BOOST bind(&StreamModeFileReceiver::handle_read, this, _1, _2));
}

void StreamModeFileReceiver::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error || bytes_transferred > 0)
	{
		_STD cout << "bytes " << bytes_transferred << _STD endl;
		FileOnDisk.async_write(_BOOST bind(&StreamModeFileReceiver::handle_write, this, _1, _2), _ASIO buffer(Buffer, bytes_transferred));
	}
	else
	{
		_STD cout << "Expected: " << FileSize << " Wroted " << BytesWrottedToDisck << _STD endl;
		_STD cout << "Read failed: " << error.message() << "\n";
		BytesWrottedToDisck == FileSize ? OnSuccess() : OnError();
	}
}

void StreamModeFileReceiver::handle_write(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		BytesWrottedToDisck += bytes_transferred;
		_ASIO async_read(*SocketPtr, _ASIO buffer(Buffer),_BOOST bind(&StreamModeFileReceiver::handle_read, this, _1, _2));
	}
	else
	{
		_STD cout << "Write failed: " << error.message() << "\n";
		OnError();
	}
}