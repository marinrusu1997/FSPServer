#pragma once

namespace fsp::protocol::message {
	struct message;
}

struct basic_validator final
{
	basic_validator() = delete;

	[[nodiscard]] static const char * validate(fsp::protocol::message::message const& req) noexcept;
};