#include "compressed_file_deliver.h"
#include "abstract_compressor.h"
#include "protocol.h"

#include <boost/bind.hpp>
#include <sstream>
#include <iomanip>

#include <iostream>

#define _ASIO ::boost::asio::
#define _FSP ::fsp::protocol::
#define CHECK_WRITE_STATUS(ec) if ((ec))	{	CleanUpHandler_();	return;	}

void CompressedModeFileDeliver::Start()
{
	std::cout << "delivering in compressed mode " << std::endl;
	AbstractFileDeliver::Start();
	FileOnDisk.async_read(boost::bind(&CompressedModeFileDeliver::handle_read, this, _1, _2), _ASIO buffer(Buffer));
}

void CompressedModeFileDeliver::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error || error == _ASIO error::eof && bytes_transferred > 0)
	{
		std::cout << "bytes read " << bytes_transferred << std::endl;
		if (error == _ASIO error::eof)
			DoneFlag = true;

		CompressedChunk = Compressor->compress({ Buffer ,bytes_transferred });

		_STD stringstream sstream;
		sstream << _STD setw(_FSP COMPRESSED_CHUNK_HEADER_LENGTH) << _STD setfill('0') << CompressedChunk.length();
		_STD string compr_len = sstream.str();

		boost::system::error_code ec;
		_ASIO write(*SocketPtr, _ASIO buffer(compr_len, compr_len.length()), ec);
		CHECK_WRITE_STATUS(ec)

		_ASIO async_write(*SocketPtr, _ASIO buffer(CompressedChunk, CompressedChunk.length()), boost::bind(&CompressedModeFileDeliver::handle_write, this, _1, _2));
	}
	else
	{
		std::cout << "read error " << error.message() << std::endl;
		CleanUpHandler_();
	}
}

void CompressedModeFileDeliver::handle_write(const boost::system::error_code& error, const size_t)
{
	if (!error)
	{
		if (DoneFlag)
		{
			std::cout << "write done, cleaning up" << std::endl;
			CleanUpHandler_();
			return;
		}
		FileOnDisk.async_read(boost::bind(&CompressedModeFileDeliver::handle_read, this, _1, _2), _ASIO buffer(Buffer));
	}
	else
	{
		std::cout << "write error " << error.message() << std::endl;
		CleanUpHandler_();
	}
}