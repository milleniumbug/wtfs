#include "common.hpp"
#include "structure.hpp"

boost::optional<std::pair<directory&, size_t>> resolve_path(
    const char* rawpath, wtfs& fs)
{
	std::string path = rawpath;
	boost::char_separator<char> sep("/");
	boost::tokenizer<decltype(sep)> tokens(path, sep);

	directory* current = &fs.root;
	ssize_t fd = 0;
	bool end = false;
	for(auto& part : tokens)
	{
		if(end)
			return boost::none;
		auto dirit = current->subdirectories.find(part);
		if(dirit != current->subdirectories.end())
		{
			current = &dirit->second;
			fd = current->directory_file;
			continue;
		}
		auto fileit = current->files.find(part);
		if(fileit != current->files.end())
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

uint64_t create_file_handle(size_t fileindex, wtfs& fs)
{
	auto& file = fs.files[fileindex];
	auto fh = std::make_unique<wtfs_file_handle>();
	fh->current_filedata =
	    std::make_pair(file.first_filedata_begin, file.first_filedata_end);
	fh->position = 0;
	fh->offset_last = 0;
	static_assert(sizeof(uint64_t) >= sizeof(uintptr_t), "");
	auto file_identifier = reinterpret_cast<uint64_t>(fh.get());
	fs.file_handles.emplace(file_identifier, std::move(fh));
	return file_identifier;
}

void destroy_file_handle(uint64_t file_handle, wtfs& fs)
{
	fs.file_handles.erase(file_handle);
}

void deallocate_file(size_t fh, wtfs& fs)
{
	// TODO: a serious implementation
}
