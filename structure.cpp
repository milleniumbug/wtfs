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
		if(auto dirit = current->lookup_directory(part))
		{
			current = &dirit.get();
			fd = current->directory_file;
			continue;
		}
		if(auto fileit = current->lookup_file(part))
		{
			end = true;
			fd = fileit.get();
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
		else if(auto dirit = current->lookup_directory(part))
		{
			current = &dirit.get();
			auto d = std::make_pair(part, current);
			retval.base = d;
			retval.parents.push_back(d);
			++retval.successfully_resolved;
		}
		else if(auto fileit = current->lookup_file(part))
		{
			end = true;
			retval.base = std::make_pair(part, fileit.get());
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

off_t allocate_chunk(size_t length, wtfs& fs)
{
	// TODO: a serious implementation
	off_t left = fs.allocator.chunk;
	fs.allocator.chunk += length;
	fs.bpb->data_end += length;
	off_t allocated_chunk = off_t(left);
	decltype(fs.chunk_cache)::iterator it;
	bool inserted;
	std::tie(it, inserted) = fs.chunk_cache.emplace(
	    allocated_chunk, mmap_alloc<chunk>(block_size * length,
	                         fs.bpb->data_offset + left * block_size, fs));
	auto& block = *it->second;
	memset(&block, 0xCC, block_size * length);
	block.prev_pointer = 0;
	block.next_pointer = 0;
	block.size = length;
	assert(inserted);
	return allocated_chunk;
}

void deallocate_chunk(off_t chunk, wtfs& fs)
{
	// TODO: a serious implementation
}

wtfs::wtfs() : root(0, *this)
{
}

chunk* wtfs::load_chunk(off_t key)
{
	auto it = chunk_cache.find(key);
	if(it != chunk_cache.end())
	{
		return it->second.get();
	}
	else
	{
		decltype(chunk_cache)::iterator it;
		bool inserted;
		std::tie(it, inserted) = chunk_cache.emplace(
		    key, mmap_alloc<chunk>(
		             block_size, bpb->data_offset + key * block_size, *this));
		assert(inserted);
		return it->second.get();
	}
}

void directory::insert(boost::string_ref component, directory dir)
{
	if(!lookup(component))
	{
		subdirectories_.emplace(component, std::move(dir));
		cached = cache_state::dirty;
	}
}

void directory::insert(boost::string_ref component, size_t file)
{
	if(!lookup(component))
	{
		files_.emplace(component, file);
		cached = cache_state::dirty;
	}
}

void directory::erase(boost::string_ref component)
{
	fill_cache();
	subdirectories_.erase(component.to_string());
	files_.erase(component.to_string());
	cached = cache_state::dirty;
}

boost::optional<directory&> directory::lookup_directory(
    boost::string_ref component)
{
	fill_cache();
	if(auto fileopt = at(subdirectories_, component.to_string()))
	{
		return fileopt->second;
	}
	return boost::none;
}

boost::optional<size_t> directory::lookup_file(boost::string_ref component)
{
	fill_cache();
	if(auto diropt = at(files_, component.to_string()))
	{
		return diropt->second;
	}
	return boost::none;
}

boost::optional<boost::variant<directory&, size_t>> directory::lookup(
    boost::string_ref component)
{
	fill_cache();
	if(auto fileopt = at(files_, component.to_string()))
	{
		return boost::variant<directory&, size_t>(fileopt->second);
	}
	if(auto diropt = at(subdirectories_, component.to_string()))
	{
		return boost::variant<directory&, size_t>(diropt->second);
	}
	return boost::none;
}

void directory::fill_cache()
{
	if(cached != cache_state::empty)
		return;

	auto& file = fs_->files[directory_file];
	file_content_iterator it(fs_->files[directory_file], *fs_);
	std::vector<char> buffer;
	buffer.reserve(it.size());
	std::copy_n(it, it.size(), std::back_inserter(buffer));
	const char* beg = buffer.data();
	const char* end = beg + buffer.size();
	while(beg != end)
	{
		size_t index;
		std::copy_n(beg, sizeof index, reinterpret_cast<char*>(&index));
		beg += sizeof index;
		boost::string_ref s(beg);
		beg += s.size();
		++beg;
		if(S_ISDIR(file.mode))
		{
			directory newdir(index, *fs_);
			subdirectories_.emplace(s, std::move(newdir));
		}
		else
		{
			files_.emplace(s, index);
		}
	}
	cached = cache_state::clean;
}

void directory::dump_cache()
{
	if(cached != cache_state::dirty)
		return;

	auto& file = fs_->files[directory_file];
	file.size = 0;
	file_content_iterator it(file, *fs_);
	const char null_terminator = '\0';
	for(auto&& x : subdirectories_)
	{
		const size_t index = x.second.directory_file;
		boost::copy(make_raw_range(index), it);
		boost::copy(x.first, it);
		std::copy(&null_terminator, &null_terminator + 1, it);
	}
	for(auto&& x : files_)
	{
		const size_t index = x.second;
		boost::copy(make_raw_range(index), it);
		boost::copy(x.first, it);
		std::copy(&null_terminator, &null_terminator + 1, it);
	}
	cached = cache_state::clean;
}

size_t directory::entries_count()
{
	fill_cache();
	return subdirectories_.size() + files_.size();
}

directory::directory(size_t directory_file, wtfs& fs)
    : directory_file(directory_file)
{
	cached = cache_state::empty;
	fs_ = &fs;
}

directory::~directory()
{
	dump_cache();
}

file_content_iterator::file_content_iterator()
    : position_(std::make_shared<position>())
{
}

file_content_iterator::file_content_iterator(wtfs_file& file, wtfs& fs)
{
	position_ = std::make_shared<position>();
	position_->fs = &fs;
	position_->file = &file;
	position_->current = file.first_chunk;
	next_chunk(file.first_chunk);
}

void file_content_iterator::next_chunk(off_t nextptr)
{
	if(nextptr == 0)
	{
		nextptr = allocate_chunk(1, *position_->fs);
		position_->chunk->next_pointer = nextptr;
		position_->file->last_chunk = nextptr;
		position_->fs->load_chunk(nextptr)->prev_pointer =
		    position_->fs->load_chunk(position_->chunk->prev_pointer)
		        ->next_pointer;
	}
	position_->chunk = position_->fs->load_chunk(nextptr);
	position_->pos = position_->chunk->data;
	off_t clusters = position_->chunk->size;
	size_t chunk_size = clusters * block_size - sizeof(chunk);
	position_->end = position_->pos + chunk_size;
}

void file_content_iterator::increment()
{
	++position_->pos;
	++position_->offset;
	auto s = size();
	position_->file->size = std::max(s, position_->offset);
	if(position_->pos == position_->end)
		next_chunk(position_->chunk->next_pointer);
}

bool file_content_iterator::equal(const file_content_iterator& other) const
{
	auto tied = [](const file_content_iterator& it)
	{
		return std::tie(
		    it.position_->pos, it.position_->chunk, it.position_->fs);
	};
	return tied(*this) == tied(other);
}

char& file_content_iterator::dereference() const
{
	char& c = *position_->pos;
	return c;
}

off_t file_content_iterator::size()
{
	return position_->file->size;
}

off_t file_content_iterator::offset()
{
	return position_->offset;
}

mmap_alloc_impl_tuple mmap_alloc_impl(size_t length, off_t offset, wtfs& fs)
{
	return mmap_alloc_impl_tuple(nullptr, length, PROT_READ | PROT_WRITE,
	    fs.filesystem_fd ? MAP_SHARED : MAP_PRIVATE | MAP_ANONYMOUS,
	    fs.filesystem_fd.get(), offset);
}