#include "Server.h"
#include "Configuration.h"
#include "connection.h"
#include <iostream>

void Server::init() {
	// Register to handle the signals that indicate when the server should exit.
		// It is safe to register for the same signal multiple times in a program,
		// provided all registration for the specified signal is made through Asio.
		signals_.add(SIGINT);
		signals_.add(SIGTERM);
	#if defined(SIGQUIT)
		signals_.add(SIGQUIT);
	#endif // defined(SIGQUIT)

		do_await_stop();
}

Server::Server(uint16_t port) :
	io_context_(),
	acceptor_(io_context_),
	signals_(io_context_),
	connection_manager_()
{
	init();
	// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
	auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();

	do_accept();
}

Server::Server(std::string_view address, std::string_view port) :
	io_context_(),
	acceptor_(io_context_),
	signals_(io_context_),
	connection_manager_()
{
	init();
	// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
	boost::asio::ip::tcp::resolver resolver(io_context_);
	boost::asio::ip::tcp::endpoint endpoint =
		*resolver.resolve(address, port).begin();
	acceptor_.open(endpoint.protocol());
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.bind(endpoint);
	acceptor_.listen();

	do_accept();
}

Server::~Server()
{
	do_await_stop();
	io_context_.stop();
	io_thread_.join();
	connection_manager_.handler().stop();
}

void Server::run() {
	static std::once_flag once_flg;

	std::call_once(once_flg, [this]() 
	{	
		this->io_thread_ = std::thread([this]() {io_context_.run(); });
	});
}

void Server::do_accept() 
{
	acceptor_.async_accept(
		[this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
			// Check whether the server was stopped by a signal before this
			// completion handler had a chance to run.
			if (!acceptor_.is_open())
				return;

			if (!ec)
				connection_manager_.start(std::make_shared<connection>(std::move(socket), connection_manager_));

			do_accept();
	});
}

void Server::do_await_stop()
{
	signals_.async_wait(
		[this](boost::system::error_code /*ec*/, int /*signo*/)
	{
		// The server is stopped by cancelling all outstanding asynchronous
		// operations. Once all operations have finished the io_context::run()
		// call will exit.
		acceptor_.close();
	});
}

