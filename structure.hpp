#pragma once

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

struct wtfs_filedata
{
	off_t next_filedata_begin;
	off_t next_filedata_end;
	char data[];
};

struct directory
{
	directory* parent;
	size_t directory_file;
	// using boost containers because they can be used with incomplete types
	boost::container::map<std::string, directory> subdirectories;
	boost::container::map<std::string, size_t> files;
};

struct wtfs
{
	std::unique_ptr<wtfs_bpb> bpb;
	std::unique_ptr<wtfs_file[]> files;
	std::map<std::pair<off_t, off_t>,
	    std::unique_ptr<wtfs_filedata, free_deleter>> filedata_cache;
	directory root;
};

boost::optional<std::pair<directory&, size_t>> resolve_path(const char* rawpath, wtfs& fs);
