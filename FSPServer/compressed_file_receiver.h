#pragma once
#include "abstract_file_receiver.h"

namespace fsp::protocol::compressors {
	struct AbstractCompressor;
}

struct CompressedModeFileReceiver final : public AbstractFileReceiver
{
	CompressedModeFileReceiver(handler&& CompletionHandler, fsp::protocol::compressors::AbstractCompressor *Decompressor,
		HANDLE OpenedFileOnDisk, uintmax_t FileSize, boost::asio::io_context& io_ctx)
		: AbstractFileReceiver(_STD move(CompletionHandler), OpenedFileOnDisk, FileSize, io_ctx),
		Decompressor(Decompressor)
	{}

	void Start() override;
private:
	fsp::protocol::compressors::AbstractCompressor *Decompressor;
	_STD string										DecompressedChunk;

	void handle_read(const boost::system::error_code& error, size_t bytes_transferred);

	void handle_write(const boost::system::error_code& error, size_t bytes_transferred);
};