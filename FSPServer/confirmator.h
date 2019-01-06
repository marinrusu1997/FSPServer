#pragma once
namespace fsp::protocol::message {
	struct message;
}

struct confirmator final
{
	[[nodiscard]] static const char * confirm(fsp::protocol::message::message const& req) noexcept;
};