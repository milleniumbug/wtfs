#pragma once

#include "ops.hpp"

struct munmap_deleter
{
	size_t length;

	munmap_deleter(size_t length) : length(length) {}
	template <typename T>
	void operator()(T* ptr)
	{
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
	char padding[block_size - 24];
};
static_assert(sizeof(wtfs_bpb) == block_size, "sizeof incorrect");
static_assert(
    std::is_trivially_copyable<wtfs_bpb>::value, "must be trivially copyable");

struct wtfs_file
{
	off_t size;
	off_t first_filedata_begin;
	off_t first_filedata_end;
	off_t last_filedata_begin;
	off_t last_filedata_end;
	mode_t mode;
	nlink_t hardlink_count;
	uid_t user;
	gid_t group;
	char data[block_size - 65];
};
static_assert(sizeof(wtfs_file) == block_size, "sizeof incorrect");
static_assert(
    std::is_trivially_copyable<wtfs_file>::value, "must be trivially copyable");

struct wtfs_filedata
{
	off_t next_filedata_begin;
	off_t next_filedata_end;
	char data[];
};

struct wtfs_file_handle
{
	std::pair<off_t, off_t> current_filedata;
	size_t position;
	off_t offset_last;
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

	private:
	char* pos;
	char* end;
	wtfs_filedata* filedata;
	wtfs* fs;
	off_t* size_;

	friend class boost::iterator_core_access;

	void next_filedata(std::pair<off_t, off_t> nextptr);
	void increment();
	bool equal(const file_content_iterator& other) const;
	char& dereference() const;
};

struct directory
{
	directory* parent;
	size_t directory_file;
	cache_state cached;
	wtfs* filesystem;
	// using boost containers because they can be used with incomplete types
	boost::container::map<std::string, directory>& subdirectories();
	boost::container::map<std::string, size_t>& files();

	boost::container::map<std::string, directory> subdirectories_;
	boost::container::map<std::string, size_t> files_;

	void fill_cache();
	void dump_cache();
};

struct wtfs
{
	unique_fd filesystem_fd;
	std::unique_ptr<wtfs_bpb> bpb;
	std::unique_ptr<wtfs_file[]> files;
	std::map<std::pair<off_t, off_t>,
	    std::unique_ptr<wtfs_filedata, free_deleter>> filedata_cache;
	directory root;
	std::map<uint64_t, std::unique_ptr<file_content_iterator>> file_handles_new;

	wtfs_filedata* load_filedata(std::pair<off_t, off_t> key);

	struct wtfs_allocator
	{
		off_t file;
		off_t filedata;
	} allocator;
};

boost::optional<std::pair<directory&, size_t>> resolve_path(
    const char* rawpath, wtfs& fs);

size_t allocate_file(wtfs& fs);
uint64_t create_file_handle(size_t fileindex, wtfs& fs);
void destroy_file_handle(uint64_t file_handle, wtfs& fs);
void deallocate_file(size_t fh, wtfs& fs);
