#include "common.hpp"

#include "structure.hpp"
#include "ops.hpp"
#include "tests.hpp"

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
	for(auto& x : dir.subdirectories())
	{
		const char* rawpath = x.first.c_str();
		filler(buf, rawpath, nullptr, 0);
	}
	for(auto& x : dir.files())
	{
		const char* rawpath = x.first.c_str();
		filler(buf, rawpath, nullptr, 0);
	}
	return 0;
}

int wtfs_mknod(const char* rawpath, mode_t mode, dev_t rdev)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	auto resolv = resolve_dirs(rawpath, fs);
	if(resolv.failed_to_resolve == 0)
		return -EEXIST;
	if(resolv.failed_to_resolve == 1 && !resolv.base)
	{
		size_t allocated_file_index = allocate_file(fs);
		auto chunk = allocate_chunk(1, fs);
		auto& file = fs.files[allocated_file_index];
		file.size = 0;
		file.first_chunk_begin = chunk.first;
		file.first_chunk_end = chunk.second;
		file.last_chunk_begin = chunk.first;
		file.last_chunk_end = chunk.second;
		file.mode = mode;
		file.hardlink_count = 1;
		// TODO: is this correct?
		file.user = ctx->uid;
		file.group = ctx->gid;
		auto last_component = path_from_rawpath(rawpath).back();
		auto& direct_parent = resolv.parents.back();
		direct_parent.second->files().emplace(
		    last_component, allocated_file_index);
		return 0;
	}
	else
	{
		return -ENOENT;
	}
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

int wtfs_rename(const char* rawfrom, const char* rawto)
{
	/*struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	auto dirs_from = resolve_dirs(rawfrom, fs);
	auto dirs_to = resolve_dirs(rawto, fs);
	auto last_component_from = dirs_from.back();
	auto last_component_to = dirs_to.at(dirs_to.size() - 2);
	if(parentfrom_opt && parentto_opt)
	{
	    auto& parentfrom = parentfrom_opt->first;
	    auto& parentto = parentto_opt->first;
	    auto dir = parentfrom.subdirectories().find(last_component_from);
	    if(dir != parentfrom.subdirectories().end())
	    {
	        parentto.subdirectories().emplace(
	            last_component_to, std::move(dir->second));
	        parentfrom.subdirectories().erase(dir);
	    }
	    auto file = parentfrom.files().find(last_component_from);
	    if(file != parentfrom.files().end())
	    {
	        parentto.files().emplace(
	            last_component_to, std::move(file->second));
	        parentfrom.files().erase(file);
	    }
	    return 0;
	}*/
	return -ENOENT;
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
		fi->fh = create_file_description(fileopt->second, fs);
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

	auto& it = *fs.file_descriptions[fi->fh];

	const auto s = std::min(static_cast<size_t>(it.size() - offset), size);
	std::copy_n(it, s, buf);
	return s;
}

int wtfs_write(const char* path, const char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	auto& it = *fs.file_descriptions[fi->fh];
	const auto s = std::min(static_cast<size_t>(it.size() - offset), size);
	std::copy_n(buf, s, it);
	return s;
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

	destroy_file_description(fi->fh, fs);
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
	fs->files = std::make_unique<wtfs_file[]>(15);
	{
		auto& f = fs->files[0];
		f.size = 4000;
		f.first_chunk_begin = 13;
		f.first_chunk_end = 13;
		f.last_chunk_begin = 13;
		f.last_chunk_end = 13;
		f.mode = S_IFDIR | 0555;
		f.hardlink_count = 1;
		f.user = 0;
		f.group = 0;
	}
	{
		auto& f = fs->files[1];
		f.size = 5000;
		f.first_chunk_begin = 10;
		f.first_chunk_end = 11;
		f.last_chunk_begin = 10;
		f.last_chunk_end = 11;
		f.mode = S_IFDIR | 0666;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		auto& f = fs->files[2];
		f.size = 5;
		f.first_chunk_begin = 14;
		f.first_chunk_end = 14;
		f.last_chunk_begin = 14;
		f.last_chunk_end = 14;
		f.mode = S_IFREG | 0740;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		auto& f = fs->files[3];
		f.size = 7000;
		f.first_chunk_begin = 16;
		f.first_chunk_end = 16;
		f.last_chunk_begin = 17;
		f.last_chunk_end = 17;
		f.mode = S_IFREG | 0740;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		auto& f = fs->files[4];
		f.size = 0;
		f.first_chunk_begin = 18;
		f.first_chunk_end = 18;
		f.last_chunk_begin = 18;
		f.last_chunk_end = 18;
		f.mode = S_IFREG | 0740;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		decltype(fs->chunk_cache)::iterator it;
		bool inserted;
		{
			std::tie(it, inserted) = fs->chunk_cache.emplace(
			    std::make_pair(10, 11),
			    std::unique_ptr<chunk, free_deleter>(
			        static_cast<chunk*>(malloc(block_size * 2))));
			assert(inserted);
			auto& block = *it->second;
			memset(&block, 'z', block_size * 2);
			memset(block.data, 'a', 5000);
			block.next_chunk_begin = 0;
			block.next_chunk_end = 0;
		}
		{
			std::tie(it, inserted) = fs->chunk_cache.emplace(
			    std::make_pair(13, 13),
			    std::unique_ptr<chunk, free_deleter>(
			        static_cast<chunk*>(malloc(block_size))));
			assert(inserted);
			auto& block = *it->second;
			memset(&block, 'z', block_size);
			memset(block.data, 'a', 4000);
			block.next_chunk_begin = 0;
			block.next_chunk_end = 0;
		}
		{
			std::tie(it, inserted) = fs->chunk_cache.emplace(
			    std::make_pair(14, 14),
			    std::unique_ptr<chunk, free_deleter>(
			        static_cast<chunk*>(malloc(block_size))));
			assert(inserted);
			auto& block = *it->second;
			memset(&block, 'z', block_size);
			memset(block.data, 'a', 5);
			block.next_chunk_begin = 0;
			block.next_chunk_end = 0;
		}
		{
			std::tie(it, inserted) = fs->chunk_cache.emplace(
			    std::make_pair(16, 16),
			    std::unique_ptr<chunk, free_deleter>(
			        static_cast<chunk*>(malloc(block_size))));
			assert(inserted);
			auto& block = *it->second;
			memset(&block, 'z', block_size);
			memset(block.data, 'b', block_size - sizeof(chunk));
			block.next_chunk_begin = 17;
			block.next_chunk_end = 17;
		}
		{
			std::tie(it, inserted) = fs->chunk_cache.emplace(
			    std::make_pair(17, 17),
			    std::unique_ptr<chunk, free_deleter>(
			        static_cast<chunk*>(malloc(block_size))));
			assert(inserted);
			auto& block = *it->second;
			memset(&block, 'z', block_size);
			memset(block.data, 'c', 7000 - (block_size - sizeof(chunk)));
			block.next_chunk_begin = 0;
			block.next_chunk_end = 0;
		}
		{
			std::tie(it, inserted) = fs->chunk_cache.emplace(
			    std::make_pair(18, 18),
			    std::unique_ptr<chunk, free_deleter>(
			        static_cast<chunk*>(malloc(block_size))));
			assert(inserted);
			auto& block = *it->second;
			memset(&block, 'z', block_size);
			block.next_chunk_begin = 0;
			block.next_chunk_end = 0;
		}
	}
	{
		directory dir;
		dir.directory_file = 1;
		dir.files().emplace("koles", 2);
		dir.files().emplace("ziom", 3);
		fs->root.subdirectories().emplace("asdf", dir);
		fs->root.files().emplace("laffo_pusty_plik", 4);
	}
	run_tests(*fs);
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
	wtfs_oper.mknod = wtfs_mknod;
	/*wtfs_oper.mkdir = wtfs_mkdir;
	wtfs_oper.symlink = wtfs_symlink;
	wtfs_oper.unlink = wtfs_unlink;
	wtfs_oper.rmdir = wtfs_rmdir;*/
	wtfs_oper.rename = wtfs_rename;
	/*wtfs_oper.link = wtfs_link;
	wtfs_oper.chmod = wtfs_chmod;
	wtfs_oper.chown = wtfs_chown;
	wtfs_oper.truncate = wtfs_truncate;*/
	wtfs_oper.open = wtfs_open;
	wtfs_oper.read = wtfs_read;
	wtfs_oper.write = wtfs_write;
	/*wtfs_oper.statfs = wtfs_statfs;*/
	wtfs_oper.release = wtfs_release;
	wtfs_oper.fsync = wtfs_fsync;
	wtfs_oper.flag_nullpath_ok = 0;
	return wtfs_oper;
}
