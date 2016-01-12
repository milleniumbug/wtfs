#include "common.hpp"
#include "structure.hpp"

std::vector<std::string> path_from_rawpath(const char* rawpath)
{
	std::string path = rawpath;
	boost::char_separator<char> sep("/");
	boost::tokenizer<decltype(sep)> tokens(path, sep);
	return std::vector<std::string>(tokens.begin(), tokens.end());
}

boost::optional<std::pair<directory&, size_t>> resolve_path(
    const char* rawpath, wtfs& fs)
{
	directory* current = &fs.root;
	ssize_t fd = 0;
	bool end = false;
	auto tokens = path_from_rawpath(rawpath);
	for(auto& part : tokens)
	{
		if(end)
			return boost::none;
		if(auto dirit = at(current->subdirectories(), part))
		{
			current = &dirit->second;
			fd = current->directory_file;
			continue;
		}
		if(auto fileit = at(current->files(), part))
		{
			end = true;
			fd = fileit->second;
		}
		else
			return boost::none;
	}
	return std::pair<directory&, size_t>(*current, fd);
}

resolve_result resolve_dirs(const char* rawpath, wtfs& fs)
{
	directory* current = &fs.root;
	bool end = false;
	auto tokens = path_from_rawpath(rawpath);
	resolve_result retval;
	retval.parents.reserve(tokens.size());
	retval.parents.emplace_back("root", current);
	retval.successfully_resolved = 1;
	retval.failed_to_resolve = 0;
	for(auto& part : tokens)
	{
		if(end)
		{
			++retval.failed_to_resolve;
			if(retval.base)
			{
				--retval.successfully_resolved;
				++retval.failed_to_resolve;
				retval.base = boost::none;
			}
		}
		else if(auto dirit = at(current->subdirectories(), part))
		{
			current = &dirit->second;
			auto d = std::make_pair(part, current);
			retval.base = d;
			retval.parents.push_back(d);
			++retval.successfully_resolved;
		}
		else if(auto fileit = at(current->files(), part))
		{
			end = true;
			retval.base = std::make_pair(part, fileit->second);
			++retval.successfully_resolved;
		}
		else
		{
			end = true;
			++retval.failed_to_resolve;
			retval.base = boost::none;
		}
	}
	if(retval.base &&
	    boost::polymorphic_strict_get<directory*>(&retval.base->second))
	{
		retval.parents.pop_back();
	}
	return retval;
}

size_t allocate_file(wtfs& fs)
{
	// TODO: a serious implementation
	return fs.allocator.file++;
}

uint64_t create_file_description(size_t fileindex, wtfs& fs)
{
	auto& file = fs.files[fileindex];
	auto fhn = std::make_unique<file_content_iterator>(file, fs);
	static_assert(sizeof(uint64_t) >= sizeof(uintptr_t), "");
	auto file_identifier = reinterpret_cast<uint64_t>(fhn.get());
	fs.file_descriptions.emplace(file_identifier, std::move(fhn));
	return file_identifier;
}

void destroy_file_description(uint64_t file_description, wtfs& fs)
{
	fs.file_descriptions.erase(file_description);
}

void deallocate_file(size_t fh, wtfs& fs)
{
	// TODO: a serious implementation
}

std::pair<off_t, off_t> allocate_chunk(size_t length, wtfs& fs)
{
	// TODO: a serious implementation
	off_t left = fs.allocator.chunk;
	off_t right = left + length - 1;
	fs.allocator.chunk += length;
	std::pair<off_t, off_t> allocated_chunk = std::make_pair(left, right);
	decltype(fs.chunk_cache)::iterator it;
	bool inserted;
	std::tie(it, inserted) = fs.chunk_cache.emplace(
	    allocated_chunk, mmap_alloc<chunk>(block_size * length,
	                         fs.bpb->data_offset + left * block_size, fs));
	auto& block = *it->second;
	memset(&block, 0xCC, block_size * length);
	block.next_chunk_begin = 0;
	block.next_chunk_end = 0;
	assert(inserted);
	return allocated_chunk;
}

void deallocate_chunk(std::pair<off_t, off_t> chunk, wtfs& fs)
{
	// TODO: a serious implementation
}

chunk* wtfs::load_chunk(std::pair<off_t, off_t> key)
{
	auto it = chunk_cache.find(key);
	if(it != chunk_cache.end())
	{
		return it->second.get();
	}
	else
	{
		// load chunk
		assert(false && "unimplemented");
		return nullptr;
	}
}

boost::container::map<std::string, directory>& directory::subdirectories()
{
	if(cached == cache_state::empty)
		fill_cache();
	return subdirectories_;
}

boost::container::map<std::string, size_t>& directory::files()
{
	if(cached == cache_state::empty)
		fill_cache();
	return files_;
}

void directory::fill_cache()
{
	cached = cache_state::clean;
}

void directory::dump_cache()
{
	cached = cache_state::clean;
}

file_content_iterator::file_content_iterator()
    : pos_(nullptr),
      end_(nullptr),
      chunk_(nullptr),
      fs_(nullptr),
      size_(nullptr),
      offset_(0)
{
}

file_content_iterator::file_content_iterator(wtfs_file& file, wtfs& fs)
{
	this->fs_ = &fs;
	this->size_ = &file.size;
	this->offset_ = 0;
	next_chunk(std::make_pair(file.first_chunk_begin, file.first_chunk_end));
}

void file_content_iterator::next_chunk(std::pair<off_t, off_t> range)
{
	if(range == make_from(range, 0, 0))
	{
		range = allocate_chunk(1, *fs_);
		chunk_->next_chunk_begin = range.first;
		chunk_->next_chunk_end = range.second;
	}
	chunk_ = fs_->load_chunk(range);
	pos_ = chunk_->data;
	off_t clusters = range.second - range.first + 1;
	size_t chunk_size = clusters * block_size - sizeof(chunk);
	end_ = pos_ + chunk_size;
}

void file_content_iterator::increment()
{
	++pos_;
	++offset_;
	auto s = *size_;
	*size_ = std::max(s, offset_);
	if(pos_ == end_)
		next_chunk(
		    std::make_pair(chunk_->next_chunk_begin, chunk_->next_chunk_end));
}

bool file_content_iterator::equal(const file_content_iterator& other) const
{
	auto tied = [](const file_content_iterator& it)
	{
		return std::tie(it.pos_, it.chunk_, it.fs_);
	};
	return tied(*this) == tied(other);
}

char& file_content_iterator::dereference() const
{
	char& c = *pos_;
	return c;
}

off_t file_content_iterator::size()
{
	return *size_;
}

mmap_alloc_impl_tuple mmap_alloc_impl(size_t length, off_t offset, wtfs& fs)
{
#ifdef WTFS_TEST1
	return mmap_alloc_impl_tuple(nullptr, length, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#else
	return mmap_alloc_impl_tuple(nullptr, length, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE, fs.filesystem_fd.get(), offset);
#endif
}