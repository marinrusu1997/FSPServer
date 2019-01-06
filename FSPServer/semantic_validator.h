#pragma once
namespace fsp::protocol::message {
	struct message;
}

struct semantic_validator final
{
	[[nodiscard]] static const char * validate(fsp::protocol::message::message& req) noexcept;
private:
	[[nodiscard]] static const char * validateAuthentication(fsp::protocol::message::message& req) noexcept;
	[[nodiscard]] static const char * validatePushFiles(fsp::protocol::message::message& req) noexcept;
	[[nodiscard]] static const char * validateQuerry(fsp::protocol::message::message& req) noexcept;
	[[nodiscard]] static const char * validateReply(fsp::protocol::message::message& req) noexcept;
}; 

