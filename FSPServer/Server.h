#pragma once
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/io_context.hpp>
#include <thread>
#include <string_view>

#include "connection_manager.h"

class Server final
{

public:
	Server(const Server&) = delete;
	Server& operator=(const Server&) = delete;

	/// Construct the server to listen on the specified TCP address and port
	Server(std::string_view address, std::string_view port);

	///Construct the server to listen on all interfaces and specified port
	explicit Server(uint16_t port);

	~Server();

	void run();

private:

	///Ititiates the signal set and does the await stop
	void init();

	/// Perform an asynchronous accept operation.
	void do_accept();

	/// Wait for a request to stop the server.
	void do_await_stop();

	/// The io_context used to perform asynchronous operations.
	boost::asio::io_context			io_context_;

	/// Thread on which io_context will run
	std::thread						io_thread_;

	/// Acceptor used to listen for incoming connections.
	boost::asio::ip::tcp::acceptor	acceptor_;

	/// The signal_set is used to register for process termination notifications.
	boost::asio::signal_set			signals_;

	/// The connection manager which owns all live connections.
	connection_manager				connection_manager_;
};

