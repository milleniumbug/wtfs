#include "common.hpp"

static_assert((__BYTE_ORDER == __LITTLE_ENDIAN), "not a little endian machine");

#include "structure.hpp"
#include "ops.hpp"

int main(int argc, char** argv)
{
	auto wtfs_oper =
#ifdef WTFS_TEST1
	    wtfs_test_operations();
#else
	    wtfs_operations();
#endif
	;
	return fuse_main(argc, argv, &wtfs_oper, nullptr);
}
