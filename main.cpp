#include "common.hpp"

static_assert((__BYTE_ORDER == __LITTLE_ENDIAN), "not a little endian machine");

#include "structure.hpp"
#include "ops.hpp"

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		std::cerr << "fuse: device file not given\n";
		return -1;
	}
	// argv + 0 and argv + 1 are valid
	auto path = std::unique_ptr<char, free_deleter>(realpath(argv[1], nullptr));
	std::rotate(argv + 1, argv + 2, argv + argc);
	argv[argc - 1] = nullptr;
	--argc;
	auto wtfs_oper =
#ifdef WTFS_TEST1
	    wtfs_test_operations();
#else
	    wtfs_operations();
#endif
	;
	return fuse_main(argc, argv, &wtfs_oper, path.get());
}
