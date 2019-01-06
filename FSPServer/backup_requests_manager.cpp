#include "backup_requests_manager.h"
#include "protocol.h"


requests_backup_manager::requests_backup_manager(simple_timer::duration	deadline) : deadline_(deadline)
{}

requests_backup_manager::~requests_backup_manager() noexcept
{
	for (const auto&[RequestID, RequestTimerPair] : backedup_requests_)
		RequestTimerPair.second->stop();
}

_NODISCARD const bool			requests_backup_manager::IsRequestIDValid(req_id_key_t const& RequestID, _STD string_view& FilePath, _STD string_view& FileOwner) const noexcept
{
	_STD lock_guard<_STD mutex> guard(mutex_);
	if (auto&& iter = this->backedup_requests_.find(RequestID); iter != this->backedup_requests_.end())
	{
		iter->second.second->stop();
		FilePath = iter->second.first.File_Path();
		FileOwner = iter->second.first.File_Owner();
		return true;
	}
	return false;
}



const size_t					requests_backup_manager::Remove(req_id_key_t const& RequestID) noexcept
{
	_STD lock_guard<_STD mutex> guard(mutex_);
	this->backedup_requests_[RequestID].second->stop();
	return this->backedup_requests_.erase(RequestID);
}

_NODISCARD const bool	requests_backup_manager::IsDownloadPending(const _STD string_view FileOwner, const _STD string_view FileName,
	_STD weak_ptr<connection>&& WeakConnection, fsp::protocol::compressions::compression RequestedCompression, _STD string&& QuerryID)
{
	_STD lock_guard<_STD mutex> guard(mutex_);
	if (auto&& iter = _STD find_if(backedup_requests_.begin(), backedup_requests_.end(), [&](const auto& pair) {return pair.second.first.File_Owner() == FileOwner
		&& pair.second.first.File_Path() == FileName; }); iter != backedup_requests_.end())
	{
		iter->second.first.AddRequest(_STD move(WeakConnection), RequestedCompression, _STD move(QuerryID));
		return true;
	}
	else
		return false;
}

