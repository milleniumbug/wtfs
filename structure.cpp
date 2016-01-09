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
		auto dirit = current->subdirectories().find(part);
		if(dirit != current->subdirectories().end())
		{
			current = &dirit->second;
			fd = current->directory_file;
			continue;
		}
		auto fileit = current->files().find(part);
		if(fileit != current->files().end())
		{
			end = true;
			fd = fileit->second;
		}
		else
			return boost::none;
	}
	return std::pair<directory&, size_t>(*current, fd);
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
    : pos(nullptr), end(nullptr), chunk_(nullptr), fs(nullptr)
{
}

file_content_iterator::file_content_iterator(wtfs_file& file, wtfs& fs)
{
	this->fs = &fs;
	this->size_ = &file.size;
	next_chunk(std::make_pair(file.first_chunk_begin, file.first_chunk_end));
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

void file_content_iterator::next_chunk(std::pair<off_t, off_t> range)
{
	chunk_ = fs->load_chunk(range);
	pos = chunk_->data;
	off_t clusters = range.second - range.first + 1;
	size_t chunk_size = clusters * block_size - sizeof(chunk_);
	end = pos + chunk_size;
}

void file_content_iterator::increment()
{
	++pos;
	if(pos == end)
		next_chunk(
		    std::make_pair(chunk_->next_chunk_begin, chunk_->next_chunk_end));
}

bool file_content_iterator::equal(const file_content_iterator& other) const
{
	auto tied = [](const file_content_iterator& it)
	{
		return std::tie(it.pos, it.chunk_, it.fs);
	};
	return tied(*this) == tied(other);
}

char& file_content_iterator::dereference() const
{
	return *pos;
}

off_t file_content_iterator::size()
{
	return *size_;
}