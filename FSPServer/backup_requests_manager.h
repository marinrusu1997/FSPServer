#pragma once
#include <map>
#include <string>
#include <forward_list>
#include <mutex>
#include "inttypes.h"
#include "simple_timer.h"
#include "fsp_compressions.h"

class connection;

class requests_backup_manager final 
{
public:
	typedef uint64_t req_id_key_t;
	struct DownloadInfo
	{
		DownloadInfo() = default;
		DownloadInfo(_STD string&& FileOwner,_STD string&& FilePath)
			:	FileOwner(_STD move(FileOwner)),
				FilePath(_STD move(FilePath))
		{}

		void AddRequest(_STD weak_ptr<connection>&& WeakConnection, fsp::protocol::compressions::compression RequestedCompression, _STD string&& RequestID)
		{
			Requests.push_front({ WeakConnection,RequestedCompression, _STD move(RequestID) });
		}
		void RemoveRequest(_STD weak_ptr<connection>&& WeakConnection)
		{
			Requests.remove_if([&](const auto& Request) {return Request.WeakConnection.lock() == WeakConnection.lock(); });
		}
		template<typename Func>
		void ForEach(Func&& f)
		{
			for (auto& Request : Requests)
			{
				if (auto SharedConnection = Request.WeakConnection.lock(); SharedConnection)
					f(_STD move(SharedConnection), Request.Compression, Request.RequestID);
			}
		}

		const _STD string_view	File_Owner() const noexcept { return FileOwner; }
		const _STD string_view	File_Path() const noexcept { return FilePath; }
		_STD string				GetLocalPath() const { return FileOwner + FilePath; }
	private:
		_STD string FileOwner;
		_STD string FilePath;
		struct Request
		{
			_STD weak_ptr<connection>					WeakConnection;
			fsp::protocol::compressions::compression	Compression;
			_STD string									RequestID;
		};
		_STD forward_list<Request>	Requests;
	};

	requests_backup_manager(simple_timer::duration	deadline);
	requests_backup_manager(requests_backup_manager const&) = delete;
	requests_backup_manager& operator=(requests_backup_manager const&) = delete;
	~requests_backup_manager() noexcept;

	template<class Func>
	void							BackupAndStartTimer(req_id_key_t const& RequestID, DownloadInfo&& DownloadInfoPtr, boost::asio::io_context& ContextForTimer, Func&& handler) noexcept
	{
		auto timer = _STD make_shared <simple_timer>(ContextForTimer);
		timer->set_handler([this, RequestID = RequestID, Handler = _STD move(handler), self = timer->shared_from_this()](const auto& error_code)
		{
			if (error_code != boost::asio::error::operation_aborted && *self)
				ForEachAndRemoveWrapper(RequestID, _STD move(Handler));
			const_cast<timer_ptr_t&>(self).reset();
		}
		);

		_STD lock_guard<_STD mutex> guard(mutex_);
		this->backedup_requests_.emplace(RequestID, _STD pair{ _STD move(DownloadInfoPtr), _STD move(timer) });
		this->backedup_requests_[RequestID].second->start(this->deadline_);
	}
	template<class Func>
	void							ForEachAndRemoveWrapper(req_id_key_t const& RequestID, Func&& f)
	{
		_STD lock_guard<_STD mutex> guard(mutex_);
		backedup_requests_[RequestID].first.ForEach(_STD forward<Func>(f));
		backedup_requests_.erase(RequestID);
	}

	/// Atomic checks if requests is valid, if so stops timer and retrieves local path
	_NODISCARD const bool			IsRequestIDValid(req_id_key_t const& RequestID,_STD string_view& FilePath, _STD string_view& FileOwner) const noexcept;
	const size_t					Remove(req_id_key_t const& RequestID) noexcept;
	_NODISCARD const bool			IsDownloadPending(const _STD string_view FileOwner, const _STD string_view FileName,
		_STD weak_ptr<connection>&& WeakConnection, fsp::protocol::compressions::compression RequestedCompression, _STD string&& QuerryID);
private:
	typedef _STD shared_ptr<simple_timer>			timer_ptr_t;
	typedef _STD pair <DownloadInfo, timer_ptr_t>	backup_t;
	typedef _STD map  <req_id_key_t, backup_t >		requests_map;
	
	mutable _STD mutex				mutex_;
	requests_map			backedup_requests_;
	simple_timer::duration	deadline_;
};
