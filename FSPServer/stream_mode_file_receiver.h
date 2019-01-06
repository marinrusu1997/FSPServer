#pragma once

#include "abstract_file_receiver.h"

struct StreamModeFileReceiver final : public AbstractFileReceiver
{
	StreamModeFileReceiver(handler&& CompletionHandler,
		HANDLE OpenedFileOnDisk, uintmax_t FileSize, boost::asio::io_context& io_ctx)
		: AbstractFileReceiver(_STD move(CompletionHandler), OpenedFileOnDisk, FileSize, io_ctx)
	{}

	void Start() override;
private:
	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);

	void handle_write(const boost::system::error_code& error, size_t bytes_transferred);
};