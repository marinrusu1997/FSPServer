#include "semantic_validator.h"
#include "message.h"
#include "fsp_commands.h"
#include "fsp_responses.h"
#include "fsp_headers.h"
#include "fsp_querry_types.h"
#include "fsp_ssl_versions.h"

using namespace fsp::protocol::commands;
using namespace fsp::protocol::responses;
using namespace fsp::protocol::headers;
using namespace fsp::protocol::content_type_formats;
using namespace fsp::protocol::message;

_NODISCARD const char * semantic_validator::validate(message& request_) noexcept
{
	if (request_.command() == SIGNIN || request_.command() == SIGNUP)
		return validateAuthentication(request_);
	if (request_.command() == PUSH_FILES)
		return validatePushFiles(request_);
	if (request_.command() == QUERRY)
		return validateQuerry(request_);

	return nullptr;
}

_NODISCARD const char * semantic_validator::validateAuthentication(message& request_) noexcept
{
	if (!request_.have_header(Nickname))
		return NICKNAME_HDR_REQUIRED;

	if (!request_.have_header(Password))
		return PASSWORD_HDR_REQUIRED;

	return nullptr;
}

_NODISCARD const char * semantic_validator::validatePushFiles(message& request_) noexcept
{
	if (request_.have_header(ContentLength))
	{
		if (!request_.have_header(ContentType))
			return CONTENT_TYPE_HDR_REQUIRED;
		if (!isValidContentTypeFormat(request_[ContentType]))
			return CONTENT_TYPE_UNSUPPORTED;
	}
	return nullptr;
}

_NODISCARD const char * semantic_validator::validateQuerry(message& request_) noexcept
{
	if (!request_.have_header(QuerryType))
		return QUERRY_TYPE_HDR_REQUIRED;

	const auto& QuerryType_ = request_[QuerryType];

	if ( (QuerryType == querries::DOWNLOAD_FILE || QuerryType == querries::REMOVE_PATH ||
		QuerryType == querries::RENAME_PATH || QuerryType_ == querries::ADD_FILE)
		&& !request_.have_header(FileName))
		return FILE_NAME_HDR_REQUIRED;

	if (QuerryType == querries::DOWNLOAD_FILE && !request_.have_header(FileOwner))
		return FILE_OWNER_HDR_REQUIRED;

	if (QuerryType == querries::DOWNLOAD_FILE && !request_.have_header(SslVersion))
		return SSL_VERSION_HDR_REQUIRED;

	if (QuerryType == querries::DOWNLOAD_FILE && !fsp::protocol::ssl_versions::IsSslVersionValid(request_[SslVersion]))
		return INVALID_SSL_VERSION;

	if (QuerryType == querries::RENAME_PATH && !request_.have_header(FileNewName))
		return FILE_NEW_NAME_HDR_REQUIRED;

	return nullptr;
}

_NODISCARD const char * semantic_validator::validateReply(fsp::protocol::message::message& request_) noexcept
{
	if (!request_.have_header(ReplyCode))
		return RPL_CODE_HDR_REQUIRED;

	if (request_[ReplyCode] == DOWNLOAD_FILE_QUERRY_APPROVED && !request_.have_header(FileSize))
		return FILE_SIZE_HDR_REQUIRED;

	return nullptr;
}