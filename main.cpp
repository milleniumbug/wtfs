#include "common.hpp"

static_assert((__BYTE_ORDER == __LITTLE_ENDIAN), "not a little endian machine");

#include "structure.hpp"
#include "ops.hpp"

#include <boost/version.hpp>

int main(int argc, char** argv)
{
	auto wtfs_oper = wtfs_operations();
	return fuse_main(argc, argv, &wtfs_oper, nullptr);
}
