#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 30

#include <map>
#include <iostream>
#include <memory>
#include <algorithm>
#include <vector>
#include <stack>
#include <queue>
#include <tuple>
#include <type_traits>
#include <experimental/string_view>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <boost/container/map.hpp>
#include <boost/optional.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/tokenizer.hpp>
#include <boost/variant.hpp>
#include <boost/variant/polymorphic_get.hpp>
#include <unistd.h>
#include <fcntl.h>
#include <endian.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>
#include <fuse.h>
