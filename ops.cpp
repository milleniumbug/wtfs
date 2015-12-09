#include "common.hpp"

#include "structure.hpp"
#include "ops.hpp"

int wtfs_getattr(const char* path, struct stat* stbuf)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);
	memset(stbuf, 0, sizeof *stbuf);

	auto fileopt = resolve_path(path, fs);
	if(fileopt)
	{
		auto& file = fs.files[fileopt->second];
		stbuf->st_mode = file.mode;
		stbuf->st_nlink = file.hardlink_count;
		stbuf->st_uid = file.user;
		stbuf->st_gid = file.group;
		stbuf->st_size = file.size;
		return 0;
	}
	else
		return -ENOENT;
}

int wtfs_access(const char* path, int mask)
{
	int res;
	res = access(path, mask);
	if(res == -1)
		return -errno;
	return 0;
}

int wtfs_readlink(const char* path, char* buf, size_t size)
{
	int res;
	res = readlink(path, buf, size - 1);
	if(res == -1)
		return -errno;
	buf[res] = '\0';
	return 0;
}

int wtfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info* fi)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	auto ind = resolve_path(path, fs);
	if(ind && ind->first.directory_file != ind->second)
		return -ENOENT;

	filler(buf, ".", nullptr, 0);
	filler(buf, "..", nullptr, 0);

	auto& dir = ind->first;
	for(auto& x : dir.subdirectories)
	{
		const char* rawpath = x.first.c_str();
		filler(buf, rawpath, nullptr, 0);
	}
	for(auto& x : dir.files)
	{
		const char* rawpath = x.first.c_str();
		filler(buf, rawpath, nullptr, 0);
	}
	return 0;
}

int wtfs_mknod(const char* path, mode_t mode, dev_t rdev)
{
	std::cout << "mknod - not implemented\n";
	return 0;
}

int wtfs_mkdir(const char* path, mode_t mode)
{
	std::cout << "mkdir - not implemented\n";
	return 0;
}

int wtfs_unlink(const char* path)
{
	int res;
	res = unlink(path);
	if(res == -1)
		return -errno;
	return 0;
}

int wtfs_rmdir(const char* path)
{
	int res;
	res = rmdir(path);
	if(res == -1)
		return -errno;
	return 0;
}

int wtfs_symlink(const char* from, const char* to)
{
	int res;
	res = symlink(from, to);
	if(res == -1)
		return -errno;
	return 0;
}

int wtfs_rename(const char* from, const char* to)
{
	int res;
	res = rename(from, to);
	if(res == -1)
		return -errno;
	return 0;
}

int wtfs_link(const char* from, const char* to)
{
	int res;
	res = link(from, to);
	if(res == -1)
		return -errno;
	return 0;
}

int wtfs_chmod(const char* path, mode_t mode)
{
	int res;
	res = chmod(path, mode);
	if(res == -1)
		return -errno;
	return 0;
}

int wtfs_chown(const char* path, uid_t uid, gid_t gid)
{
	int res;
	res = lchown(path, uid, gid);
	if(res == -1)
		return -errno;
	return 0;
}

int wtfs_truncate(const char* path, off_t size)
{
	int res;
	res = truncate(path, size);
	if(res == -1)
		return -errno;
	return 0;
}

#ifdef HAVE_UTIMENSAT
int wtfs_utimens(const char* path, const struct timespec ts[2])
{
	int res;
	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if(res == -1)
		return -errno;
	return 0;
}
#endif
int wtfs_open(const char* path, struct fuse_file_info* fi)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	auto fileopt = resolve_path(path, fs);
	if(fileopt)
	{
		fi->fh = create_file_handle(fileopt->second, fs);
		// przeszukiwanie obecnie nie jest wspierane
		fi->nonseekable = true;
	}
	else
		return -ENOENT;
	return 0;
}

int wtfs_read(const char* path, char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	auto& handle = *fs.file_handles[fi->fh];
	assert(offset == handle.offset_last);
	auto it = fs.filedata_cache.find(handle.current_filedata);
	if(it != fs.filedata_cache.end())
	{
		const char* source_begin = it->second->data + handle.position;
		off_t clusters = it->first.second - it->first.first;
		size_t filedata_size = clusters * block_size - sizeof(wtfs_filedata);
		const char* source_end =
		    source_begin + std::min(handle.position + size, filedata_size);

		std::ptrdiff_t s = copy(source_begin, source_end, buf, buf + size);
		if(s >= 0)
		{
			// end of destination data
			// user buffer too small
			handle.position += size;
			handle.offset_last += size;
			return size; // I did what I could
		}
		else if(s < 0)
		{
			// end of source data
			// either EOF or need to copy more from the next block
			size_t copied = std::distance(source_begin, source_end);
			handle.position += copied;
			handle.offset_last += copied;
			return copied;
		}
	}
	else
	{
		// fill the cache
		assert(false && "unimplemented");
	}
}

int wtfs_write(const char* path, const char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi)
{
	int fd;
	int res;
	(void)fi;
	fd = open(path, O_WRONLY);
	if(fd == -1)
		return -errno;
	res = pwrite(fd, buf, size, offset);
	if(res == -1)
		res = -errno;
	close(fd);
	return res;
}

int wtfs_statfs(const char* path, struct statvfs* stbuf)
{
	int res;
	res = statvfs(path, stbuf);
	if(res == -1)
		return -errno;
	return 0;
}

int wtfs_release(const char* path, struct fuse_file_info* fi)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	destroy_file_handle(fi->fh, fs);
	return 0;
}

int wtfs_fsync(const char* path, int isdatasync, struct fuse_file_info* fi)
{
	/* Just a stub.  This method is optional and can safely be left
	   unimplemented */
	(void)path;
	(void)isdatasync;
	(void)fi;
	return 0;
}

void* wtfs_init(fuse_conn_info* conn)
{
	auto fs = std::make_unique<wtfs>();
	fs->bpb = std::make_unique<wtfs_bpb>();
	fs->files = std::make_unique<wtfs_file[]>(3);
	{
		auto& f = fs->files[0];
		f.size = 4000;
		f.first_filedata_begin = 13;
		f.first_filedata_end = 13;
		f.last_filedata_begin = 13;
		f.last_filedata_end = 13;
		f.mode = S_IFDIR | 0555;
		f.hardlink_count = 1;
		f.user = 0;
		f.group = 0;
	}
	{
		auto& f = fs->files[1];
		f.size = 5000;
		f.first_filedata_begin = 10;
		f.first_filedata_end = 11;
		f.last_filedata_begin = 10;
		f.last_filedata_end = 11;
		f.mode = S_IFDIR | 0666;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		auto& f = fs->files[2];
		f.size = 5;
		f.first_filedata_begin = 14;
		f.first_filedata_end = 14;
		f.last_filedata_begin = 14;
		f.last_filedata_end = 14;
		f.mode = S_IFREG | 0740;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		decltype(fs->filedata_cache)::iterator a1, a2, a3;
		bool inserted;
		std::tie(a1, inserted) = fs->filedata_cache.emplace(
		    std::make_pair(10, 11),
		    std::unique_ptr<wtfs_filedata, free_deleter>(
		        static_cast<wtfs_filedata*>(malloc(block_size * 2))));
		assert(inserted);
		std::tie(a2, inserted) = fs->filedata_cache.emplace(
		    std::make_pair(13, 13),
		    std::unique_ptr<wtfs_filedata, free_deleter>(
		        static_cast<wtfs_filedata*>(malloc(block_size))));
		assert(inserted);
		std::tie(a3, inserted) = fs->filedata_cache.emplace(
		    std::make_pair(14, 14),
		    std::unique_ptr<wtfs_filedata, free_deleter>(
		        static_cast<wtfs_filedata*>(malloc(block_size))));
		assert(inserted);
		{
			auto& block = *a1->second;
			memset(&block, 'z', block_size * 2);
			memset(block.data, 'a', 5000);
			block.next_filedata_begin = 0;
			block.next_filedata_end = 0;
		}
		{
			auto& block = *a3->second;
			memset(&block, 'z', block_size);
			memset(block.data, 'a', 4000);
			block.next_filedata_begin = 0;
			block.next_filedata_end = 0;
		}
		{
			auto& block = *a3->second;
			memset(&block, 'z', block_size);
			memset(block.data, 'a', 5);
			block.next_filedata_begin = 0;
			block.next_filedata_end = 0;
		}
	}
	{
		directory dir;
		dir.directory_file = 1;
		dir.files.emplace("koles", 2);
		fs->root.subdirectories.emplace("asdf", dir);
	}

	// sample data for a filesystem
	return fs.release();
}

void wtfs_destroy(void* private_data)
{
	std::unique_ptr<wtfs> p(static_cast<wtfs*>(private_data));
}

struct fuse_operations wtfs_operations()
{
	struct fuse_operations wtfs_oper = {};
	wtfs_oper.init = wtfs_init;
	wtfs_oper.destroy = wtfs_destroy;
	wtfs_oper.getattr = wtfs_getattr;
	/*wtfs_oper.access = wtfs_access;
	wtfs_oper.readlink = wtfs_readlink;*/
	wtfs_oper.readdir = wtfs_readdir;
	/*wtfs_oper.mknod = wtfs_mknod;
	wtfs_oper.mkdir = wtfs_mkdir;
	wtfs_oper.symlink = wtfs_symlink;
	wtfs_oper.unlink = wtfs_unlink;
	wtfs_oper.rmdir = wtfs_rmdir;
	wtfs_oper.rename = wtfs_rename;
	wtfs_oper.link = wtfs_link;
	wtfs_oper.chmod = wtfs_chmod;
	wtfs_oper.chown = wtfs_chown;
	wtfs_oper.truncate = wtfs_truncate;*/
	wtfs_oper.open = wtfs_open;
	wtfs_oper.read = wtfs_read;       /*
	       wtfs_oper.write = wtfs_write;
	       wtfs_oper.statfs = wtfs_statfs;*/
	wtfs_oper.release = wtfs_release; /*
	 wtfs_oper.fsync = wtfs_fsync;
	 wtfs_oper.flag_nullpath_ok = 0;*/
	return wtfs_oper;
}