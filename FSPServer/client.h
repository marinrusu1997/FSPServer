#pragma once
#include "VirtualFileSystemTree.h"

#include <string>
#include <string_view>
#include <atomic>
#include <protocol.h>

namespace fsp::impl::server::models {
	class client final
	{
	public:
		typedef VirtualFileSystemTree<_STD string>::string_t	path_t;

		/// State in which connection can be
		enum class State : int8_t
		{
			connected,
			logged,
			processing
		};

		client() : state_(State::connected), remote_file_system_(fsp::protocol::PATH_SEPARATOR_CHR,fsp::protocol::PATH_ROOT,this->nickname_)
		{}
		client(client const&) = delete;
		client& operator=(client const&) = delete;


		auto& state() noexcept { return state_; }
		const auto& state() const noexcept { return state_; }
		void  state(State state) { state_.store(state); }

		void nickname(std::string_view nick) { nickname_ = nick; }
		_NODISCARD std::string_view nickname() const noexcept { return nickname_; }

		auto& RemoteFileSystem() noexcept { return remote_file_system_; }
	private:
		/// Nickname of the client
		std::string				nickname_;
		/// State of the client
		std::atomic<State>		state_;
		/// Virtual file system of the client shared directory in ASCII characters
		VirtualFileSystemTree<_STD string> remote_file_system_;
	};
}
