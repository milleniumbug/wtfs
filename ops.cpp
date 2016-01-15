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
		stbuf->st_ino = static_cast<ino_t>(fileopt->second);
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
	// res = access(path, mask);
	std::cerr << "not implemented: " << __func__ << "\n";
	return -42;
}

int wtfs_readlink(const char* path, char* buf, size_t size)
{
	// res = readlink(path, buf, size - 1);
	// buf[res] = '\0';
	std::cerr << "not implemented: " << __func__ << "\n";
	return -43;
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
	dir.enumerate_entries([&](auto&& x)
	    {
		    const char* rawpath = x.first.c_str();
		    filler(buf, rawpath, nullptr, 0);
		});
	return 0;
}

int wtfs_mknod(const char* rawpath, mode_t mode, dev_t rdev)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	assert(!S_ISDIR(mode));
	auto resolv = resolve_dirs(rawpath, fs);
	if(resolv.failed_to_resolve == 0)
		return -EEXIST;
	if(resolv.failed_to_resolve == 1 && !resolv.base)
	{
		size_t allocated_file_index = allocate_file(fs);
		auto chunk = allocate_chunk(1, fs);
		auto& file = fs.files[allocated_file_index];
		file.size = 0;
		file.first_chunk = chunk;
		file.last_chunk = chunk;
		file.mode = mode;
		file.hardlink_count = 1;
		// TODO: is this correct?
		file.user = ctx->uid;
		file.group = ctx->gid;
		auto last_component = path_from_rawpath(rawpath).back();
		auto& direct_parent = resolv.parents.back().second;
		direct_parent->insert(last_component, allocated_file_index);
		return 0;
	}
	else
	{
		return -ENOENT;
	}
}

int wtfs_mkdir(const char* rawpath, mode_t mode)
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
		file.first_chunk = chunk;
		file.last_chunk = chunk;
		file.mode = mode | S_IFDIR;
		file.hardlink_count = 1;
		// TODO: is this correct?
		file.user = ctx->uid;
		file.group = ctx->gid;
		directory dir(allocated_file_index, fs);
		auto last_component = path_from_rawpath(rawpath).back();
		auto& direct_parent = resolv.parents.back().second;
		direct_parent->insert(last_component, std::move(dir));
		return 0;
	}
	else
	{
		return -ENOENT;
	}
}

int wtfs_unlink(const char* path)
{
	// struct fuse_context* ctx = fuse_get_context();
	// auto& fs = *static_cast<wtfs*>(ctx->private_data);
	std::cerr << "not implemented: " << __func__ << "\n";
	return -45;
}

int wtfs_rmdir(const char* rawpath)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	// handle EACCESS, ENOENT, ENOTDIR, EPERM
	auto resolv = resolve_dirs(rawpath, fs);
	if(resolv.failed_to_resolve > 0)
		return -ENOENT; // or ENOTDIR TODO

	{
		auto for_dir = [&](directory* dir)
		{
			auto& direct_parent = resolv.parents.back().second;
			auto last_component = resolv.base->first;
			if(dir->entries_count() > 0)
				return -ENOTEMPTY;
			direct_parent->erase(last_component);
			return 0;
		};
		auto for_file = [&](size_t inode)
		{
			return -ENOTDIR;
		};
		return boost::apply_visitor(
		    make_lambda_visitor<int>(for_dir, for_file), resolv.base->second);
	}
}

int wtfs_symlink(const char* from, const char* to)
{
	// res = symlink(from, to);
	std::cerr << "not implemented: " << __func__ << "\n";
	return -47;
}

int wtfs_rename(const char* rawfrom, const char* rawto)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	auto resolv_from = resolve_dirs(rawfrom, fs);
	auto resolv_to = resolve_dirs(rawto, fs);
	std::cerr << "not implemented: " << __func__ << "\n";
	return -47;
}

int wtfs_link(const char* rawfrom, const char* rawto)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	auto resolv_from = resolve_dirs(rawfrom, fs);
	auto resolv_to = resolve_dirs(rawto, fs);
	// TODO: Handle EACCES, EDQUOT, ENAMETOOLONG
	if(resolv_to.failed_to_resolve == 0)
		return -EEXIST;
	if(resolv_from.failed_to_resolve > 0 || resolv_to.failed_to_resolve > 1)
		return -ENOTDIR; // or -ENOENT TODO: these two cases are conflated
	{
		auto for_dir = [&](directory* dir)
		{
			return -EPERM;
		};
		auto for_file = [&](size_t inode)
		{
			auto& file = fs.files[inode];
			if(file.hardlink_count == std::numeric_limits<nlink_t>::max())
				return -EMLINK;
			auto last_component = path_from_rawpath(rawto).back();
			auto& parent_to = *resolv_to.parents.back().second;
			parent_to.insert(last_component, inode);
			++file.hardlink_count;
			return 0;
		};
		return boost::apply_visitor(make_lambda_visitor<int>(for_dir, for_file),
		    resolv_from.base->second);
	}
}

int wtfs_chmod(const char* path, mode_t mode)
{
	// res = chmod(path, mode);
	std::cerr << "not implemented: " << __func__ << "\n";
	return -49;
}

int wtfs_flush(const char* rawpath, struct fuse_file_info* fi)
{
	// TODO: implement
	std::cerr << "not implemented: " << __func__ << "\n";
	return 0;
}

int wtfs_chown(const char* path, uid_t uid, gid_t gid)
{
	// res = lchown(path, uid, gid);
	std::cerr << "not implemented: " << __func__ << "\n";
	return -51;
}

int wtfs_truncate(const char* rawpath, const off_t new_size)
{
	struct fuse_context* ctx = fuse_get_context();
	auto& fs = *static_cast<wtfs*>(ctx->private_data);

	auto resolv = resolve_dirs(rawpath, fs);
	// TODO: more errors
	if(resolv.failed_to_resolve > 0)
		return -ENOENT;

	{
		auto& file_or_dir = resolv.base->second;
		auto for_dir = [&](directory* dir)
		{
			return -EISDIR;
		};
		auto for_file = [&](size_t inode)
		{
			auto& file = fs.files[inode];
			auto old_size = file.size;
			if(new_size <= old_size)
			{
				file.size = new_size;
				return 0;
			}
			else
			{
				// it = std::fill_n(it, new_size - old_size, 0);
				// return 0;
				return -44;
			}
		};
		return boost::apply_visitor(
		    make_lambda_visitor<int>(for_dir, for_file), file_or_dir);
	}
}

#ifdef HAVE_UTIMENSAT
int wtfs_utimens(const char* path, const struct timespec ts[2])
{
	/* don't use utime/utimes since they follow symlinks */
	// res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	std::cerr << "not implemented: " << __func__ << "\n";
	return -53;
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
	assert(it.offset() == offset);
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
	it = std::copy_n(buf, size, it);
	return size;
}

int wtfs_statfs(const char* path, struct statvfs* stbuf)
{
	// res = statvfs(path, stbuf);
	std::cerr << "not implemented: " << __func__ << "\n";
	return -54;
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

auto fill_bpb = [](wtfs_bpb& bpb)
{
	const char header[] = "WTFS";
	copy(std::begin(header), std::end(header), std::begin(bpb.header),
	    std::end(bpb.header));
	bpb.version = 1;
	bpb.fdtable_offset = block_size * 17;
	bpb.data_offset = bpb.fdtable_offset + block_size * 15000;
	bpb.data_end = 10000;
};

auto fill_rest = [](wtfs& fs, wtfs_bpb& bpb)
{
	{
		auto& f = fs.files[0];
		f.size = 0;
		f.first_chunk = 13;
		f.last_chunk = 13;
		f.mode = S_IFDIR | 0755;
		f.hardlink_count = 1;
		f.user = 0;
		f.group = 0;
	}
	{
		auto& f = fs.files[1];
		f.size = 0;
		f.first_chunk = 10;
		f.last_chunk = 10;
		f.mode = S_IFDIR | 0777;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		auto& f = fs.files[2];
		f.size = 0;
		f.first_chunk = 14;
		f.last_chunk = 14;
		f.mode = S_IFREG | 0740;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		auto& f = fs.files[3];
		f.size = 9000;
		f.first_chunk = 16;
		f.last_chunk = 17;
		f.mode = S_IFREG | 0740;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		auto& f = fs.files[4];
		f.size = 35791394;
		f.first_chunk = 18;
		f.last_chunk = 18;
		f.mode = S_IFREG | 0777;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		auto& f = fs.files[5];
		f.size = 0;
		f.first_chunk = 8;
		f.last_chunk = 8;
		f.mode = S_IFDIR | 0740;
		f.hardlink_count = 1;
		f.user = 1000;
		f.group = 1000;
	}
	{
		decltype(fs.chunk_cache)::iterator it;
		bool inserted;
		auto push_chunk = [&](off_t p, off_t s) -> chunk&
		{
			std::tie(it, inserted) = fs.chunk_cache.emplace(
			    off_t(p), mmap_alloc<chunk>(block_size * s,
			                  bpb.data_offset + p * block_size, fs));
			assert(inserted);
			auto& block = *it->second;
			memset(&block, 'z', block_size * s);
			block.size = s;
			return block;
		};
		{
			const off_t p = 10;
			const off_t s = 2;
			auto& block = push_chunk(p, s);
			block.prev_pointer = 0;
			block.next_pointer = 0;
		}
		{
			const off_t p = 13;
			const off_t s = 1;
			auto& block = push_chunk(p, s);
			block.prev_pointer = 0;
			block.next_pointer = 0;
		}
		{
			const off_t p = 14;
			const off_t s = 1;
			auto& block = push_chunk(p, s);
			block.prev_pointer = 0;
			block.next_pointer = 0;
			memset(block.data, 'a', 5);
		}
		{
			const off_t p = 16;
			const off_t s = 1;
			auto& block = push_chunk(p, s);
			block.prev_pointer = 0;
			block.next_pointer = 17;
			memset(block.data, 'b', block_size - sizeof(chunk));
		}
		{
			const off_t p = 17;
			const off_t s = 1;
			auto& block = push_chunk(p, s);
			block.prev_pointer = 16;
			block.next_pointer = 0;
		}
		{
			const off_t p = 18;
			const off_t s = 9700 - p;
			auto& block = push_chunk(p, s);
			block.prev_pointer = 0;
			block.next_pointer = 0;
			std::generate(block.data, block.data + 35791394,
			    [c = unsigned('a')]() mutable
			    {
				    ++c;
				    if(c > 'a' + 17)
					    c = 'a';
				    return c;
				});
		}
		{
			const off_t p = 8;
			const off_t s = 1;
			auto& block = push_chunk(p, s);
			block.prev_pointer = 0;
			block.next_pointer = 0;
		}
	}
	{
		directory asdf_dir(1, fs);
		asdf_dir.insert("laffo_pusty_plik", 2);
		asdf_dir.insert("ziom", 3);
		directory laffo_pusty_katalog_dir(5, fs);
		fs.root.insert("asdf", std::move(asdf_dir));
		fs.root.insert("dlugiplik", 4);
		fs.root.insert(
		    "laffo_pusty_katalog", std::move(laffo_pusty_katalog_dir));
		fs.root.directory_file = 0;
	}
	fs.allocator.file = 5;
	fs.allocator.chunk = 30;
	run_tests(fs);
};

std::unique_ptr<wtfs> wtfs_initialize(
    fuse_conn_info* conn, const bool fill_with_sample_data)
{
	struct fuse_context* ctx = fuse_get_context();
	auto path = static_cast<const char*>(ctx->private_data);

	auto fs = std::make_unique<wtfs>();
	fs->filesystem_fd.reset(open(path, O_RDWR));
	perror("open");
	fs->size = fs->filesystem_fd
	               ? lseek(fs->filesystem_fd.get(), 0, SEEK_END)
	               : std::numeric_limits<decltype(fs->size)>::max();
	perror("lseek");
	fs->bpb = mmap_alloc<wtfs_bpb>(block_size, 0, *fs);
	auto& bpb = *fs->bpb;
	if(fill_with_sample_data)
		fill_bpb(bpb);
	const char* expected = "WTFS";
	const char* expected_end = expected + sizeof(bpb.header);
	if(!std::equal(expected, expected_end, bpb.header) || bpb.version != 1)
		return nullptr;
	const off_t file_count =
	    (bpb.data_offset - bpb.fdtable_offset) / block_size;
	fs->files = mmap_alloc<wtfs_file[]>(
	    file_count * block_size, bpb.fdtable_offset, *fs);

	fs->root.directory_file = 0;
	// TODO: serious implementation
	// allocator
	{
		wtfs_file* beg = fs->files.get();
		wtfs_file* end = beg + file_count;
		auto it = std::find_if(beg, end, [](wtfs_file& file)
		    {
			    return file.hardlink_count == 0;
			});
		assert(it != end);
		fs->allocator.file = std::distance(beg, it);
		fs->allocator.filepool_size = file_count;
		fs->allocator.chunk = bpb.data_end;
		fs->allocator.chunkpool_size =
		    (fs->size - bpb.data_offset) / block_size;
	}
	if(fill_with_sample_data)
		fill_rest(*fs, bpb);
	return fs;
}

void* wtfs_init(fuse_conn_info* conn)
{
#ifdef WTFS_TEST1
	{
		wtfs_initialize(conn, true);
	}
#endif
	return wtfs_initialize(conn, false).release();
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
	// wtfs_oper.access = wtfs_access;
	wtfs_oper.readlink = wtfs_readlink;
	wtfs_oper.readdir = wtfs_readdir;
	wtfs_oper.mknod = wtfs_mknod;
	wtfs_oper.mkdir = wtfs_mkdir;
	wtfs_oper.symlink = wtfs_symlink;
	wtfs_oper.unlink = wtfs_unlink;
	wtfs_oper.rmdir = wtfs_rmdir;
	wtfs_oper.flush = wtfs_flush;
	wtfs_oper.rename = wtfs_rename;
	wtfs_oper.link = wtfs_link;
	wtfs_oper.chmod = wtfs_chmod;
	wtfs_oper.chown = wtfs_chown;
	wtfs_oper.truncate = wtfs_truncate;
	wtfs_oper.open = wtfs_open;
	wtfs_oper.read = wtfs_read;
	wtfs_oper.write = wtfs_write;
	wtfs_oper.statfs = wtfs_statfs;
	wtfs_oper.release = wtfs_release;
	wtfs_oper.fsync = wtfs_fsync;
	wtfs_oper.flag_nullpath_ok = 0;
	return wtfs_oper;
}