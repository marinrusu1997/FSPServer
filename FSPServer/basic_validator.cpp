#include "basic_validator.h"
#include "message.h"
#include "fsp_responses.h"

using namespace fsp::protocol::responses;
using namespace fsp::protocol::headers;
using namespace fsp::protocol::message;


_NODISCARD const char * basic_validator::validate(message const& req) noexcept
{
	if (!req.have_header(CommandId)) 
		return CMDID_HDR_REQUIRED;
	
	return nullptr;
}