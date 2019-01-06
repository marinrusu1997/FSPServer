#include "stream_file_deliver.h"
#include <boost/bind.hpp>

#include <iostream>

void StreamModeFileDeliver::Start()
{
	_STD cout << "Delivering in stream mode " << _STD endl;
	AbstractFileDeliver::Start();
	FileOnDisk.async_read(boost::bind(&StreamModeFileDeliver::handle_read, this, _1, _2), boost::asio::buffer(Buffer));
}

void StreamModeFileDeliver::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error || error == boost::asio::error::eof && bytes_transferred > 0)
	{
		_STD cout << "bytes read " << bytes_transferred << std::endl;
		if (error == boost::asio::error::eof)
			DoneFlag = true;
		boost::asio::async_write(*SocketPtr, boost::asio::buffer(Buffer, bytes_transferred), boost::bind(&StreamModeFileDeliver::handle_write, this, _1, _2));
	}
	else
	{
		std::cout << "read error " << error.message() << std::endl;
		CleanUpHandler_();
	}
}

void StreamModeFileDeliver::handle_write(const boost::system::error_code& error, const size_t bytes_wroted)
{
	if (!error)
	{
		if (DoneFlag)
		{
			_STD cout << "Done write, Cleanup " << std::endl;
			CleanUpHandler_();
			return;
		}
		FileOnDisk.async_read(boost::bind(&StreamModeFileDeliver::handle_read, this, _1, _2), boost::asio::buffer(Buffer));
	}
	else
	{
		_STD cout << "write error " << error.message() << std::endl;
		CleanUpHandler_();
	}
}