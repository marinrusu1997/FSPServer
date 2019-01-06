#pragma once
#include "abstract_file_deliver.h"

struct StreamModeFileDeliver final : public AbstractFileDeliver
{
	StreamModeFileDeliver(HANDLE OpenedFileOnDisk, boost::asio::io_context& io_ctx, CleanUpHandler&& Handler)
		: AbstractFileDeliver(OpenedFileOnDisk, io_ctx, _STD forward<CleanUpHandler>(Handler))
	{}

	void Start()	override;
private:
	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);

	void handle_write(const boost::system::error_code& error, const size_t bytes_wroted);
};