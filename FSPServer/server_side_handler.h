#pragma once
#include "message.h"
#include <forward_list>
class connection;

namespace fsp::impl::server::detail {
	class server_side_handler final : public  _STD enable_shared_from_this<server_side_handler>
	{
	public:
		void handle_message(fsp::protocol::message::message& Request, _STD shared_ptr<connection>&& Connection);
		void stop();
	private:
		struct DownloadInfo
		{
			_STD string RequestID;
			_STD string FileOwner;
			_STD string FilePath;
			_STD string	SslVersion;
			_STD string Compressions;
		};

		void resolve_querry(fsp::protocol::message::message& Request, _STD shared_ptr<connection>& Connection);

		void do_signin(const _STD string_view RequestID, const _STD string_view login, const _STD string_view password, _STD weak_ptr<connection> const& connection);
		void do_signup(const _STD string_view RequestID, const _STD string_view login, const _STD string_view password, _STD weak_ptr<connection> const& connection);
		void do_push_files(const _STD string_view RequestID, const _STD unique_ptr<_STD forward_list<_STD string>> files, _STD weak_ptr<connection> const& connection);
		void do_delete_account(const _STD string_view RequestID, _STD weak_ptr<connection> const& connection);
		void do_add_file(const _STD string_view RequestID,const _STD string_view FileName, _STD weak_ptr<connection> const& connection);
		void do_remove_path(const _STD string_view RequestID, const _STD string_view FilePath, _STD weak_ptr<connection> const& connection);
		void do_remame_path(const _STD string_view RequestID, const _STD string_view OldFilePath, const _STD string_view NewName, _STD weak_ptr<connection> const& connection);
		void do_download_file(DownloadInfo&& downloadInfo, _STD weak_ptr<connection> const& connection);
		void do_logout(std::weak_ptr<connection> const& RequestID);
	};
}