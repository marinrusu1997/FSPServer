#include "connection_manager.h"
#include "connection.h"
#include "server_side_handler.h"
#include "Configuration.h"

#include "basic_validator.h"
#include "semantic_validator.h"
#include "transition_validator.h"

#include "confirmator.h"

#include "fsp_commands.h"
#include "fsp_responses.h"
#include "protocol.h"


connection::connection(boost::asio::ip::tcp::socket&& socket, connection_manager& manager)
	:	stream_(std::move(socket)),
		strand_(stream_.get_io_context()),
		read_buffer_(),
		parser_(),
		message_buffer_(),
		client_(),
		deadline_(stream_.get_io_context(),
			strand_.wrap([this,self_weak = weak_from_this()](const auto& error)
			{
				if (!self_weak.expired())
				{
					if (error != boost::asio::error::operation_aborted && this->deadline_)
							this->stop();
				}
			})),
		manager_(manager),
		stopped_()
	{}


void connection::start()
{
	start_read();
}

void connection::stop()
{
	if (!stopped_.test_and_set(_STD memory_order_acquire)) {
		stream_.close(); 
		deadline_.stop(); 
		manager_.notify_disconnection(shared_from_this());
	}
}


void connection::start_shared_write(const _STD string_view BufferView, const uint64_t BufferID) noexcept
{
	async_shared_write(BufferView, BufferID, 0);
}

void connection::async_shared_write(const _STD string_view BufferView, const uint64_t BufferID, const size_t OffsetFromWhereToWrite) noexcept
{
	assert(OffsetFromWhereToWrite < BufferView.size());
	const auto data = BufferView.data() + OffsetFromWhereToWrite;
	const auto size = BufferView.size() - OffsetFromWhereToWrite;
	stream_.async_send(boost::asio::buffer(data, size),
		this->strand_.wrap(
			[this, BufferView = BufferView, BufferID = BufferID, OffsetFromWhereToWrite = OffsetFromWhereToWrite, SelfWeak = weak_from_this()]
	(const auto& ec, const auto& BytesTransferred){
		if (!SelfWeak.expired())
		{
			if (!ec)
			{
				if (const_cast<size_t&>(OffsetFromWhereToWrite) += BytesTransferred; OffsetFromWhereToWrite != BufferView.size())
				{
					this->async_shared_write(BufferView, BufferID, OffsetFromWhereToWrite);
				}
				else
				{
					this->manager_.notify_shared_write_done(BufferID);
				}
			}
			else
			{
				this->stop();
			}
		}
	})
	);
}

void connection::start_write(_STD string& MovableBuffer) noexcept
{
	async_write(MovableBuffer, 0);
}

void connection::async_write(_STD string& Buffer, const size_t OffsetFromWhereToWrite) noexcept
{
	assert(OffsetFromWhereToWrite < Buffer.size());
	const auto data = Buffer.data() + OffsetFromWhereToWrite;
	const auto size = Buffer.size() - OffsetFromWhereToWrite;
	stream_.async_send(boost::asio::buffer(data, size),
		this->strand_.wrap(
			[this, Buffer = std::move(Buffer), OffsetFromWhereToWrite = OffsetFromWhereToWrite, SelfWeak = weak_from_this()](const auto& ec, const auto& BytesTransferred){
				if (!SelfWeak.expired())
				{
					if (!ec)
					{
						if (const_cast<size_t&>(OffsetFromWhereToWrite) += BytesTransferred; OffsetFromWhereToWrite != Buffer.size())
						{
							this->async_write(const_cast<_STD string&>(Buffer), OffsetFromWhereToWrite);
						}
					}
					else
					{
						this->stop();
					}
				}
		})
	);
}

void connection::start_write(const char * const C_StaticAllocatedString) noexcept
{
	async_write(C_StaticAllocatedString, 0);
}

void connection::async_write(const char * const C_StaticAllocatedString, const size_t OffsetFromWhereToWrite) noexcept
{
	assert(OffsetFromWhereToWrite < strlen(C_StaticAllocatedString));
	const auto data = C_StaticAllocatedString + OffsetFromWhereToWrite;
	const auto size = strlen(C_StaticAllocatedString) - OffsetFromWhereToWrite;
	stream_.async_send(boost::asio::buffer(data, size),
		this->strand_.wrap(
			[this, C_StaticAllocatedString = C_StaticAllocatedString, OffsetFromWhereToWrite = OffsetFromWhereToWrite, SelfWeak = weak_from_this()]
	(const auto& ec, const auto& BytesTransferred){
		if (!SelfWeak.expired())
		{
			if (!ec)
			{
				if (const_cast<size_t&>(OffsetFromWhereToWrite) += BytesTransferred; OffsetFromWhereToWrite != strlen(C_StaticAllocatedString))
				{
					this->async_write(C_StaticAllocatedString, OffsetFromWhereToWrite);
				}
			}
			else
			{
				this->stop();
			}
		}
	})
	);
}

void connection::start_read() noexcept
{
	using namespace fsp::protocol::message::parsers;
	using namespace fsp::protocol::message::builders;
	using namespace fsp::protocol::responses;
	using namespace fsp::protocol;

	stream_.async_read_some(boost::asio::buffer(this->read_buffer_.buffer()),
		this->strand_.wrap([this, SelfWeak = weak_from_this()](const auto& ec, const auto& BytesTransferred){
			if (!SelfWeak.expired())
			{
				if (!ec)
				{
					this->read_buffer_.current_size(BytesTransferred);
					bool SyntacticErrorSentBefore = false;

					while (this->read_buffer_.can_read())
					{
						auto&&[result, iter] = parser_.parse(this->message_buffer_, this->read_buffer_.begin_read_iter(), this->read_buffer_.end_read_iter());

						this->read_buffer_.consume(iter);

						if (result == message_parser::good) {
							this->read_action();
							SyntacticErrorSentBefore = false;
						}
						else if (result == message_parser::bad && SyntacticErrorSentBefore == false) {
							this->start_write(StringReplyMessage().SetReplyCode(SYNTACTIC_ERROR).SetRequestID(CMD_ID_WHEN_NO_CMDID_PRESENT).SetEndOfProtocolHeader().build());
							SyntacticErrorSentBefore = true;
						}
						else if (result == message_parser::indeterminate) {
							this->read_indeterminate();
							break;
						}

						this->message_buffer_.clear();
					}

					this->start_read();
				}
				else
				{
					this->stop();
				}
			}
		})
	);
}


void connection::read_action()
{
	using fsp::protocol::message::builders::StringReplyMessage;

	deadline_.stop(); // stop deadline timer, we got valid message
	if (message_buffer_.command() == fsp::protocol::commands::HEARTBEAT)
	{
		start_write(fsp::protocol::HEARTBEAT_MESSAGE);
		return;
	}
	if (auto result = basic_validator::validate(message_buffer_); result != nullptr)
	{
		start_write(StringReplyMessage().SetReplyCode(result).SetRequestID(fsp::protocol::CMD_ID_WHEN_NO_CMDID_PRESENT).SetEndOfProtocolHeader().build());
		return;
	}
	if (auto result = transition_validator::validate(message_buffer_, client()); result != nullptr)
	{
		start_write(StringReplyMessage().SetReplyCode(result).SetRequestID(message_buffer_[fsp::protocol::headers::CommandId]).SetEndOfProtocolHeader().build());
		return;
	}
	if (auto result = semantic_validator::validate(message_buffer_); result != nullptr)
	{
		start_write(StringReplyMessage().SetReplyCode(result).SetRequestID(message_buffer_[fsp::protocol::headers::CommandId]).SetEndOfProtocolHeader().build());
		return;
	}
	if (message_buffer_.command() == fsp::protocol::commands::REPLY)
	{
		manager_.resolve_download_reply(message_buffer_);
		return;
	}

	start_write(StringReplyMessage().SetReplyCode(confirmator::confirm(message_buffer_)).SetRequestID(message_buffer_[fsp::protocol::headers::CommandId]).SetEndOfProtocolHeader().build());

	manager_.handler().handle_message(message_buffer_, shared_from_this());
}

void connection::read_indeterminate()
{
	deadline_.start(boost::posix_time::seconds(
		std::stoi(Configuration::get_instance().get_config_settings().try_get(
			persistency::keys::server::SEC_TO_W8_ON_INCOMPLETE_MSG, { "30" }).front())
	)); // must be readed from property file, maybe during runtime to change it dinamically
}



