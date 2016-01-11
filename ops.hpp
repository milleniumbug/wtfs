#pragma once

struct fuse_operations wtfs_operations();

template <typename RandomAccessIterator1, typename RandomAccessIterator2>
std::ptrdiff_t copy(RandomAccessIterator1 begin1, RandomAccessIterator1 end1,
    RandomAccessIterator2 begin2, RandomAccessIterator2 end2)
{
	auto l = std::distance(begin1, end1);
	auto r = std::distance(begin2, end2);
	std::copy_n(begin1, std::min(l, r), begin2);
	return l - r;
}

template <typename T, typename FN, typename F0, typename FP>
void arif(T value, FN negative, F0 zero, FP positive)
{
	if(value > 0)
	{
		positive();
		return;
	}
	if(value < 0)
	{
		negative();
		return;
	}
	zero();
}

class unique_fd
{
	private:
	int fd_;

	public:
	unique_fd(int fd = -1) : fd_(fd) {}
	unique_fd(unique_fd&& other)
	{
		fd_ = other.fd_;
		other.fd_ = -1;
	}

	unique_fd& operator=(unique_fd other)
	{
		std::swap(fd_, other.fd_);
		return *this;
	}

	~unique_fd() { reset(); }
	unique_fd(const unique_fd& other) = delete;

	explicit operator bool() { return fd_ != -1; }
	int get() { return fd_; }
	void reset() { reset(-1); }
	void reset(int fd)
	{
		if(fd_ != -1)
			close(fd_);
		fd_ = fd;
	}
};

template <typename Map, typename Key>
boost::optional<typename Map::reference> at(Map& map, Key&& key)
{
	auto it = map.find(std::forward<Key>(key));
	if(it != map.end())
		return *it;
	else
		return boost::none;
}

template <typename Map, typename Key>
boost::optional<typename Map::const_reference> at(const Map& map, Key&& key)
{
	auto it = map.find(std::forward<Key>(key));
	if(it != map.end())
		return *it;
	else
		return boost::none;
}