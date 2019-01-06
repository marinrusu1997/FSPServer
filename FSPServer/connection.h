#pragma once
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "simple_timer.h"
#include "client.h"
#include "mutable_buffer.h"
#include "msg_parser.h"
#include "message.h"

class connection_manager;


class connection final : 
	public std::enable_shared_from_this<connection>
{
public:
	/// Construct a connection with the given socket and manager
	connection(boost::asio::ip::tcp::socket&& socket, connection_manager& manager);
	connection(const connection&) = delete;
	connection& operator=(const connection&) = delete;

	/// Start the first asynchronous operation for the connection.
	void start();

	/// Stop all asynchronous operations associated with the connection.
	void stop();

	/// Public method so clients can write on this connection
	/// We take a rvalue reference, it's intended to anounce the called
	/// that we will move the actual content of the buffer to our internal buffer
	void start_write(_STD string& MovableBuffer) noexcept;

	/// This write is intended only for C strings allocated in static bloc of memory
	void start_write(const char * const C_StaticAllocatedString) noexcept;

	/// Get asociated client model with this connection
	auto& client() { return client_; }

	/// Get manager of this connection
	auto& manager() { return manager_; }

	auto& io_context() { return stream_.get_io_context(); }

private:
	friend class connection_manager;
	/// Private method for shared write, intended to work with connection manager
	void start_shared_write(const _STD string_view BufferView, const uint64_t BufferID) noexcept;
	void async_shared_write(const _STD string_view BufferView, const uint64_t BufferID, const size_t OffsetFromWhereToWrite) noexcept;

	/// Private method, this will actually do the real write
	void async_write(_STD string& Buffer, const size_t OffsetFromWhereToWrite)noexcept;

	/// Private method, intended to do real work only for Static String Allocated strings
	void async_write(const char * const C_StaticAllocatedString, const size_t OffsetFromWhereToWrite) noexcept;

	/// Private method for reading, clients doesn't need this function
	/// We internally always are reading for some messages to come to us
	void start_read() noexcept;
	void read_action();
	void read_indeterminate();


	/// Stream for transport layer
	boost::asio::ip::tcp::socket					stream_;
	/// Strand responsible for dispatching completion handlers
	boost::asio::io_context::strand					strand_;
	/// Raw buffer for storring bytes received over network
	mutable_buffer<char, static_cast<size_t>(5120)>	read_buffer_;
	/// Parser for getting message from raw bytes
	fsp::protocol::message::parsers::message_parser parser_;
	/// Message obtained after parsing
	fsp::protocol::message::message					message_buffer_;
	/// State of the client and his files
	fsp::impl::server::models::client				client_;
	/// Timer for read operations
	simple_timer									deadline_;
	/// Manager of this connection
	connection_manager&								manager_;
	/// Flag which idicates if this connection was already stopped
	_STD atomic_flag								stopped_;
};


