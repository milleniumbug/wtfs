#include "common.hpp"
#include "structure.hpp"

boost::optional<std::pair<directory&, size_t>> resolve_path(const char* rawpath, wtfs& fs)
{
    std::string path = rawpath;
    boost::char_separator<char> sep("/");
    boost::tokenizer<decltype(sep)> tokens(path, sep);

    directory* current = &fs.root;
    ssize_t fd = -1;
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
        {
            end = true;
            fd = fileit->second;
        }
    }
    return std::pair<directory&, size_t>(*current, fd);
}
