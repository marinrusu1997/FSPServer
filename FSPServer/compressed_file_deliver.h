#pragma once
#include "abstract_file_deliver.h"

namespace fsp::protocol::compressors {
	struct AbstractCompressor;
}

struct CompressedModeFileDeliver final : public AbstractFileDeliver
{
	CompressedModeFileDeliver(HANDLE OpenedFileOnDisk, boost::asio::io_context& io_ctx, CleanUpHandler&& Handler,
		fsp::protocol::compressors::AbstractCompressor *Compressor)
		:	AbstractFileDeliver(OpenedFileOnDisk,io_ctx,_STD forward<CleanUpHandler>(Handler)),
			CompressedChunk(),
			Compressor(Compressor)
	{}

	void Start() override;

private:
	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);

	void handle_write(const boost::system::error_code& error, const size_t bytes_wroted);

	_STD string										CompressedChunk;
	fsp::protocol::compressors::AbstractCompressor *Compressor;
};