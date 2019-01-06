#include "compressed_file_receiver.h"
#include "abstract_compressor.h"
#include <boost/bind.hpp>

#include <iostream>

#define _ASIO	::boost::asio::
#define _BOOST	::boost::
#define _FSP	::fsp::protocol::

#define ON_ERROR_EXIT(error_code) if ((error_code)) {	BytesWrottedToDisck == FileSize ? OnSuccess() : OnError();	return;	}
#define CHECK_FILE_DOWNLOAD_SUCCESS { BytesWrottedToDisck == FileSize ? OnSuccess() : OnError();	return;	}

void CompressedModeFileReceiver::Start()
{
	AbstractFileReceiver::Start();
	assert(Decompressor != nullptr);

	_STD cout << "start compressed mode " << _STD endl;

	_BOOST system::error_code ec;
	_ASIO read(*SocketPtr, _ASIO buffer(Buffer, _FSP COMPRESSED_CHUNK_HEADER_LENGTH), ec);
	ON_ERROR_EXIT(ec)

	Buffer[_FSP COMPRESSED_CHUNK_HEADER_LENGTH] = '\0';
	_ASIO async_read(*SocketPtr, _ASIO buffer(Buffer, _STD atoll(Buffer)), _BOOST bind(&CompressedModeFileReceiver::handle_read, this, _1, _2));
}

void CompressedModeFileReceiver::handle_read(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error || bytes_transferred > 0)
	{
		_STD cout << "bytes " << bytes_transferred << _STD endl;
		try {
			DecompressedChunk = Decompressor->decompress({ Buffer, bytes_transferred });
			FileOnDisk.async_write(_BOOST bind(&CompressedModeFileReceiver::handle_write, this, _1, _2), _ASIO buffer(DecompressedChunk, DecompressedChunk.length()));
		}
		catch (_STD exception const& e) {
			_STD cout << "handle_read() exception " << e.what() << _STD endl;
			CHECK_FILE_DOWNLOAD_SUCCESS
		}
	}
	else
	{
		_STD cout << "Expected: " << FileSize << " Wroted " << BytesWrottedToDisck << _STD endl;
		_STD cout << "Read failed: " << error.message() << "\n";
		CHECK_FILE_DOWNLOAD_SUCCESS
	}
}

void CompressedModeFileReceiver::handle_write(const boost::system::error_code& error, size_t bytes_transferred)
{
	if (!error)
	{
		BytesWrottedToDisck += bytes_transferred;
		_STD cout << "FileSize " << FileSize << " Wroted " << BytesWrottedToDisck << " Remains " << FileSize - BytesWrottedToDisck << _STD endl;

		_BOOST system::error_code ec;
		_ASIO read(*SocketPtr, _ASIO buffer(Buffer, _FSP COMPRESSED_CHUNK_HEADER_LENGTH), ec);
		ON_ERROR_EXIT(ec)

		Buffer[_FSP COMPRESSED_CHUNK_HEADER_LENGTH] = '\0';
		_ASIO async_read(*SocketPtr, _ASIO buffer(Buffer, _STD atoll(Buffer)), _BOOST bind(&CompressedModeFileReceiver::handle_read, this, _1, _2));
	}
	else
	{
		_STD cout << "Write failed: " << error.message() << "\n";
		CHECK_FILE_DOWNLOAD_SUCCESS
	}
}