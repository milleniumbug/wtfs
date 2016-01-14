#pragma once

#include "ops.hpp"

struct munmap_deleter
{
	size_t length;

	munmap_deleter(size_t length) : length(length) {}
	munmap_deleter() = default;
	template <typename T>
	void operator()(T* ptr)
	{
		msync(ptr, length, MS_SYNC);
		munmap(ptr, length);
	}
};

struct free_deleter
{
	template <typename T>
	void operator()(T* ptr)
	{
		free(ptr);
	}
};

constexpr size_t block_size = 4096;

enum class cache_state
{
	clean,
	dirty,
	empty
};

struct wtfs_bpb
{
	char jmp_instruction[3];
	char header[4];
	char version;
	off_t fdtable_offset;
	off_t data_offset;
	off_t data_end;
	char padding[block_size - 32];
};
static_assert(sizeof(wtfs_bpb) == block_size, "sizeof incorrect");
static_assert(
    std::is_trivially_copyable<wtfs_bpb>::value, "must be trivially copyable");

struct wtfs_file
{
	off_t size;
	off_t first_chunk;
	off_t last_chunk;
	mode_t mode;
	nlink_t hardlink_count;
	uid_t user;
	gid_t group;
	char data[block_size - 49];
};
static_assert(sizeof(wtfs_file) == block_size, "sizeof incorrect");
static_assert(
    std::is_trivially_copyable<wtfs_file>::value, "must be trivially copyable");

struct chunk
{
	off_t xor_pointer;
	off_t size;
	char data[];
};

struct wtfs;

class file_content_iterator
    : public boost::iterator_facade<file_content_iterator, char,
          boost::single_pass_traversal_tag>
{
	public:
	file_content_iterator();
	file_content_iterator(wtfs_file& file, wtfs& fs);
	off_t size();
	off_t offset();

	private:
	struct position
	{
		char* pos;
		char* end;
		struct chunk* chunk;
		off_t offset;
		off_t previous;
		wtfs* fs;
		wtfs_file* file;
	};
	// TODO
	std::shared_ptr<position> position_;

	friend class boost::iterator_core_access;

	void next_chunk(off_t nextptr);
	void increment();
	bool equal(const file_content_iterator& other) const;
	char& dereference() const;
};

struct directory
{
	size_t directory_file;

	// using boost containers because they can be used with incomplete types

	template <typename Function>
	void enumerate_entries(Function f)
	{
		fill_cache();
		for(auto&& x : subdirectories_)
			f(x);
		for(auto&& x : files_)
			f(x);
	}

	void insert(boost::string_ref component, directory dir);
	void insert(boost::string_ref component, size_t file);
	size_t entries_count();
	void erase(boost::string_ref component);

	boost::optional<directory&> lookup_directory(boost::string_ref component);
	boost::optional<size_t> lookup_file(boost::string_ref component);
	boost::optional<boost::variant<directory&, size_t>> lookup(
	    boost::string_ref component);

	void fill_cache();
	void dump_cache();

	// Big Five
	directory(size_t directory_file, wtfs& fs);
	~directory();
	directory(const directory&) = delete;
	directory& operator=(const directory&) = delete;
	directory(directory&&) = default;
	directory& operator=(directory&&) = default;

	private:
	boost::container::map<std::string, directory> subdirectories_;
	boost::container::map<std::string, size_t> files_;
	cache_state cached;
	wtfs* fs_;
};

struct wtfs
{
	unique_fd filesystem_fd;
	std::unique_ptr<wtfs_bpb, munmap_deleter> bpb;
	std::unique_ptr<wtfs_file[], munmap_deleter> files;
	std::map<off_t, std::unique_ptr<chunk, munmap_deleter>> chunk_cache;
	directory root;
	std::map<uint64_t, std::unique_ptr<file_content_iterator>>
	    file_descriptions;
	off_t size;

	chunk* load_chunk(off_t key);

	struct wtfs_allocator
	{
		off_t file;
		off_t chunk;
		size_t filepool_size;
		size_t chunkpool_size;
	} allocator;

	wtfs();
};

boost::optional<std::pair<directory&, size_t>> resolve_path(
    const char* rawpath, wtfs& fs);

struct resolve_result
{
	std::vector<std::pair<std::string, directory*>> parents;
	boost::optional<std::pair<std::string, boost::variant<directory*, size_t>>>
	    base;
	size_t successfully_resolved;
	size_t failed_to_resolve;
};

resolve_result resolve_dirs(const char* rawpath, wtfs& fs);
std::vector<std::string> path_from_rawpath(const char* rawpath);

off_t allocate_chunk(size_t length, wtfs& fs);
void deallocate_chunk(off_t chunk, wtfs& fs);
size_t allocate_file(wtfs& fs);
uint64_t create_file_description(size_t fileindex, wtfs& fs);
void destroy_file_description(uint64_t file_description, wtfs& fs);
void deallocate_file(size_t fh, wtfs& fs);

namespace detail
{
template <typename T>
struct SuppressTemplateDeduction
{
	typedef T type;
};

template <typename T>
struct DependentFalse
{
	static const bool value = false;
};
}

template <typename T,
    typename std::enable_if<!std::is_array<T>::value>::type* = nullptr>
std::unique_ptr<T, munmap_deleter> mmap_unique(
    void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	using dependent_void_pointer =
	    typename std::conditional<detail::DependentFalse<T>::value, T,
	        void*>::type;
	using Uniq = std::unique_ptr<T, munmap_deleter>;
	void* allocated =
	    mmap(dependent_void_pointer(addr), length, prot, flags, fd, offset);
	if(allocated == MAP_FAILED)
	{
		perror("mmap");
		return Uniq(nullptr);
	}
	return Uniq(
	    static_cast<typename Uniq::pointer>(allocated), munmap_deleter(length));
}

template <typename T,
    typename std::enable_if<std::is_array<T>::value &&
                            std::extent<T>::value == 0>::type* = nullptr>
std::unique_ptr<typename std::remove_extent<T>::type[], munmap_deleter>
mmap_unique(
    void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	using dependent_void_pointer =
	    typename std::conditional<detail::DependentFalse<T>::value, T,
	        void*>::type;
	using P = typename std::remove_extent<T>::type;
	using Uniq = std::unique_ptr<P[], munmap_deleter>;
	void* allocated =
	    mmap(dependent_void_pointer(addr), length, prot, flags, fd, offset);
	if(allocated == MAP_FAILED)
	{
		perror("mmap");
		return Uniq(nullptr);
	}
	return Uniq(
	    static_cast<typename Uniq::pointer>(allocated), munmap_deleter(length));
}

template <typename T, typename... Args,
    typename std::enable_if<std::is_array<T>::value &&
                            std::extent<T>::value != 0>::type* = nullptr>
void mmap_unique(Args&&... args)
{
	static_assert(detail::DependentFalse<T>::value,
	    "mmap_unique of array with a known bound is disallowed");
}

using mmap_alloc_impl_tuple = std::tuple<void*, size_t, int, int, int, off_t>;
mmap_alloc_impl_tuple mmap_alloc_impl(size_t length, off_t offset, wtfs& fs);

template <typename T>
std::unique_ptr<T, munmap_deleter> mmap_alloc(
    size_t length, off_t offset, wtfs& fs)
{
	using std::get;
	auto res = mmap_alloc_impl(length, offset, fs);
	return mmap_unique<T>(get<0>(res), get<1>(res), get<2>(res), get<3>(res),
	    get<4>(res), get<5>(res));
}

template <typename T>
boost::iterator_range<char*> make_raw_range(T& t)
{
	return boost::make_iterator_range(
	    reinterpret_cast<char*>(std::addressof(t)),
	    reinterpret_cast<char*>(std::addressof(t)) + sizeof(T));
}

template <typename T>
boost::iterator_range<const char*> make_raw_range(const T& t)
{
	return boost::make_iterator_range(
	    reinterpret_cast<const char*>(std::addressof(t)),
	    reinterpret_cast<const char*>(std::addressof(t)) + sizeof(T));
}