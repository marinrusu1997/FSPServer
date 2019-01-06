#pragma once

namespace fsp::impl::server::models {
	class client;
}
namespace fsp::protocol::message {
	struct message;
}

struct transition_validator final
{
	[[nodiscard]] static const char * validate(fsp::protocol::message::message const& req, fsp::impl::server::models::client const& client_) noexcept;
};