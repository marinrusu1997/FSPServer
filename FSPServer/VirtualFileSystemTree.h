#ifndef _VIRTUAL_FILE_SYSTEM_TREE_HEADER_
#define _VIRTUAL_FILE_SYSTEM_TREE_HEADER_

#include <forward_list>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <filesystem>

#include <iostream>

#include <atomic>
#include <mutex>
class SpinLock {
	_STD atomic_flag locked = ATOMIC_FLAG_INIT;
public:
	void lock() {
		while (locked.test_and_set(_STD memory_order_acquire)) { ; }
	}
	void unlock() {
		locked.clear(_STD memory_order_release);
	}
};

#define VFS_CRITICAL_SECTION_START(spin_lock)	{ \
													_STD lock_guard<SpinLock> lock { (spin_lock) };
#define VFS_CRITICAL_SECTION_END				}

template<class String>
class VirtualFileSystemTree final
{
public:
	typedef String	string_t;
private:
	typedef _STD forward_list<string_t>		path_components_container_t;
	typedef typename string_t::value_type	char_t;
	typedef typename string_t::traits_type	traits_t;
private:
	template<class StringType>
	struct Node
	{
		typedef StringType									string_t;
		typedef _STD forward_list<Node*>					nodes_container_t;

		string_t					MyName_;
		nodes_container_t			MyChildrens_;
		Node						*MyParent_;
		bool						Cached_;
		string_t const&				Owner_;
		const char_t				PathSeparator_;

		Node(string_t&& Name, Node* parent, string_t const& Owner, const char_t PathSeparator) : 
			MyName_(_STD move(Name)), 
			MyParent_(parent), Cached_(false),
			Owner_(Owner),
			PathSeparator_(PathSeparator)
		{
			if (MyParent_)
				MyParent_->AddChildren(this);
		}
		Node(Node const&) = delete;
		Node& operator=(Node const&) = delete;

		~Node()
		{
			if (Cached_)
			{
				string_t LocalPath;
				LocalPathToMe(LocalPath);
				_STD error_code ec;
				_STD filesystem::remove(LocalPath,ec);
				if (ec)
					_STD cout << "Failed to delete " << LocalPath << " " << ec.message() << _STD endl;
			}
			if (MyParent_)
				MyParent_->RemoveChildren(this);
			auto TempChildrens(MyChildrens_);
			for (auto c : TempChildrens)
				delete c;
		}

		void	AddChildren(Node* Children) noexcept { MyChildrens_.push_front(Children); }
		void	RemoveChildren(Node* Children) noexcept { MyChildrens_.remove(Children); }
		Node*	GetChildren(const string_t& Name) const noexcept
		{
			const auto&& iter = _STD find_if(MyChildrens_.begin(), MyChildrens_.end(), [&](const auto CurrentNode) {return CurrentNode->MyName_ == Name; });
			if (iter != MyChildrens_.end())
				return *iter;
			return nullptr;
		}
		bool	IsLeaf() const noexcept { return MyChildrens_.empty(); }
		void	MyPath(string_t& Path) const
		{
			PathToMe(Path);
			//if (MyName_ != string_t(PathSeparator_))
			Path.append(MyName_);
		}
		void	PathToMe(string_t& Path) const
		{
			Node * CurrentParrent = MyParent_;
			if (CurrentParrent)
			{
				while (CurrentParrent->MyParent_)
				{
					Path += PathSeparator_;
					Path.append(CurrentParrent->MyName_.rbegin(), CurrentParrent->MyName_.rend());
					CurrentParrent = CurrentParrent->MyParent_;
				}
			}
			Path += PathSeparator_;
			_STD reverse(Path.begin(), Path.end());
		}
		void	LocalPathToMe(string_t& Path) const
		{
			Node * CurrentParrent = MyParent_;
			if (CurrentParrent)
			{
				while (CurrentParrent->MyParent_)
				{
					Path += PathSeparator_;
					Path.append(CurrentParrent->MyName_.rbegin(), CurrentParrent->MyName_.rend());
					CurrentParrent = CurrentParrent->MyParent_;
				}
			}
			Path += PathSeparator_;
			Path.append(Owner_.rbegin(), Owner_.rend());
			_STD reverse(Path.begin(), Path.end());
			Path.append(MyName_);
		}
	};
public:
	VirtualFileSystemTree(const char_t PathSeparator, string_t&& Root, string_t const& Owner) 
		:	root_(new Node<string_t>(_STD move(Root), nullptr, Owner, PathSeparator)),
			separator_(PathSeparator),
			owner_(Owner)
	{}

	~VirtualFileSystemTree()
	{
		delete root_;
	}

	enum class PathType : int8_t { FILE, DIRECTORY, GENERIC };

	void Add(const _STD basic_string_view<char_t, traits_t> Path)
	{
		path_components_container_t PathComponents;
		get_components(Path, PathComponents);
		bool Done = false;
		VFS_CRITICAL_SECTION_START(spin_lock_)
			do_add(root_, PathComponents, Done);
		VFS_CRITICAL_SECTION_END
	}

	template <template<class, class...> class SequenceContainer, class... Args>
	void Get(SequenceContainer<string_t, Args...>& PathsContainer, const PathType PathType = PathType::FILE) const
	{
		string_t CurrentPath(root_->MyName_);
		for (auto Child : root_->MyChildrens_)
			VFS_CRITICAL_SECTION_START(spin_lock_)
				do_get_paths(Child, CurrentPath, PathsContainer, PathType);
			VFS_CRITICAL_SECTION_END
	}

	_NODISCARD const bool Find(const _STD basic_string_view<char_t, traits_t> Path,bool& Cached) const
	{
		VFS_CRITICAL_SECTION_START(spin_lock_)
			if (const Node<string_t>* NodeToFind = do_find_node(Path); NodeToFind != nullptr)
			{
				Cached = NodeToFind->Cached_;
				return true;
			}
		VFS_CRITICAL_SECTION_END
		return false;
	}

	_NODISCARD const bool Remove(const _STD basic_string_view<char_t, traits_t> Path)
	{
		VFS_CRITICAL_SECTION_START(spin_lock_)
			if (const Node<string_t>* NodeToDelete = do_find_node(Path); NodeToDelete != nullptr)
			{
				delete NodeToDelete;
				return true;
			}
		VFS_CRITICAL_SECTION_END
		return false;
	}

	_NODISCARD const bool Rename(const _STD basic_string_view<char_t, traits_t> Path, const _STD basic_string_view<char_t, traits_t> NewPath)
	{
		VFS_CRITICAL_SECTION_START(spin_lock_)
			if (const Node<string_t>* NodeToRename = do_find_node(Path); NodeToRename != nullptr)
			{
				auto&& FileName = filename(NewPath);
				if (FileName.empty())
					return false;

				string_t OldLocalPath;
				if (NodeToRename->Cached_)
					NodeToRename->LocalPathToMe(OldLocalPath);

				const_cast<Node<string_t>*>(NodeToRename)->MyName_ = FileName;
				
				if (NodeToRename->Cached_)
				{
					_STD error_code ec;
					auto NewLocalPath = _STD filesystem::path(OldLocalPath).relative_path().append(FileName);
					_STD filesystem::rename(OldLocalPath, NewLocalPath ,ec);
					if (ec)
					{
						_STD cout << "Failed to rename " << OldLocalPath << " to " << NewLocalPath.string() << " " << ec.message() << _STD endl;
					}
				}

				return true;
			}
		VFS_CRITICAL_SECTION_END
		return false;
	}

	_NODISCARD const bool SetCached(const _STD basic_string_view<char_t, traits_t> Path, const bool Cached)
	{
		VFS_CRITICAL_SECTION_START(spin_lock_)
			if (const Node<string_t>* NodeToFind = do_find_node(Path); NodeToFind != nullptr)
			{
				const_cast<Node<string_t>*>(NodeToFind)->Cached_ = Cached;
				return true;
			}
		VFS_CRITICAL_SECTION_END
		return false;
	}

	_NODISCARD const bool Empty() const noexcept
	{
		VFS_CRITICAL_SECTION_START(spin_lock_)
			return root_->MyChildrens_.empty();
		VFS_CRITICAL_SECTION_END
	}
private:
	void		get_components(const _STD basic_string_view<char_t, traits_t> Path, path_components_container_t& PathComponents) const
	{
		boost::split(PathComponents, Path, [this](const auto c) {return c == this->separator_; });
		PathComponents.remove_if([](const auto& file) {return file.empty(); });
	}

	string_t	filename(const _STD basic_string_view<char_t, traits_t> Path) const
	{
		auto Rbegin = Path.rbegin();

		while (Rbegin != Path.rend() && *Rbegin != separator_)
			++Rbegin;

		return string_t(Rbegin == Path.rend() ? Path.begin() : Rbegin.base(), Path.end());
	}

	PathType	get_file_class(const Node<string_t>* CurrentPathComponent)
	{
		if (CurrentPathComponent->IsLeaf())
			return PathType::FILE;
		return PathType::DIRECTORY;
	}

	void		do_add(Node<string_t> *Parent, path_components_container_t& PathComponents, bool done)
	{
		if (PathComponents.empty())
			return;

		auto&		CurrentPathComponent = PathComponents.front();
		const auto	Childrens = Parent->MyChildrens_;

		for (auto Children : Childrens)
		{
			if (Children->MyName_ == CurrentPathComponent)
			{
				PathComponents.pop_front();
				if (PathComponents.empty())
				{
					new Node<string_t>(_STD move(CurrentPathComponent), Children, owner_, separator_);
					done = true;
					return;
				}
				else {
					do_add(Children, PathComponents, done);
				}
			}
			if (done)
				break;
		}

		if (PathComponents.empty())
			return;

		auto NewTreePathComponent = new Node<string_t>(_STD move(CurrentPathComponent), Parent, owner_, separator_);

		PathComponents.pop_front();

		do_add(NewTreePathComponent, PathComponents, done);
	}

	template <template<class, class...> class SequenceContainer, class... Args>
	void		do_get_paths(const Node<string_t> *Parent, string_t& CurrentPath, SequenceContainer<string_t, Args...>& PathsContainer, const PathType PathType) const
	{
		CurrentPath.append(Parent->MyName_);
		CurrentPath += separator_;
		const auto Childrens = Parent->MyChildrens_;
		for (auto Child : Childrens)
		{
			do_get_paths(Child, CurrentPath, PathsContainer, PathType);
		}

		CurrentPath.pop_back();
		if (PathType == PathType::GENERIC)
			std::generate_n(std::inserter(PathsContainer, PathsContainer.begin()), 1, [&]() -> string_t {return { CurrentPath }; });
		else if (PathType == PathType::FILE && Parent->IsLeaf())
			std::generate_n(std::inserter(PathsContainer, PathsContainer.begin()), 1, [&]() -> string_t {return { CurrentPath }; });
		else if (PathType == PathType::DIRECTORY && !Parent->IsLeaf())
			std::generate_n(std::inserter(PathsContainer, PathsContainer.begin()), 1, [&]() -> string_t {return { CurrentPath }; });

		CurrentPath.erase(CurrentPath.length() - Parent->MyName_.length());
	}

	template <class... Args>
	void		do_get_paths(const Node<string_t> *Parent, string_t& CurrentPath, _STD forward_list<string_t, Args...>& PathsContainer, const PathType PathType) const
	{
		CurrentPath.append(Parent->MyName_);
		CurrentPath += separator_;
		const auto Childrens = Parent->MyChildrens_;
		for (auto Child : Childrens)
		{
			do_get_paths(Child, CurrentPath, PathsContainer, PathType);
		}

		CurrentPath.pop_back();
		if (PathType == PathType::GENERIC)
			PathsContainer.emplace_front(CurrentPath);
		else if (PathType == PathType::FILE && Parent->IsLeaf())
			PathsContainer.emplace_front(CurrentPath);
		else if (PathType == PathType::DIRECTORY && !Parent->IsLeaf())
			PathsContainer.emplace_front(CurrentPath);

		CurrentPath.erase(CurrentPath.length() - Parent->MyName_.length());
	}

	_NODISCARD const bool				do_find_path(const Node<string_t> *Parent, path_components_container_t& PathComponents, const Node<string_t> **FoundNode) const
	{
		if (PathComponents.empty())
		{
			if (FoundNode)
				*FoundNode = Parent;
			return true;
		}

		auto Children = Parent->GetChildren(PathComponents.front());
		PathComponents.pop_front();
		if (Children)
			return do_find_path(Children, PathComponents, FoundNode);
		return false;
	}

	_NODISCARD const Node<string_t>*	do_find_node(const _STD basic_string_view<char_t, traits_t> Path) const
	{
		path_components_container_t PathComponents;
		get_components(Path, PathComponents);
		if (PathComponents.empty())
			return nullptr;
		const Node<string_t>* Node = nullptr;
		if (!do_find_path(root_, PathComponents, &Node))
			return nullptr;
		return Node;
	}


	mutable SpinLock	spin_lock_;
	Node<string_t> 		*root_;
	string_t const&		owner_;
	const char_t		separator_;
};

#endif