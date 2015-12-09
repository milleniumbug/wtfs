#pragma once

struct fuse_operations wtfs_operations();

template <typename RandomAccessIterator1, typename RandomAccessIterator2>
std::ptrdiff_t copy(RandomAccessIterator1 begin1, RandomAccessIterator1 end1,
    RandomAccessIterator2 begin2, RandomAccessIterator2 end2)
{
	auto l = std::distance(begin1, end1);
	auto r = std::distance(begin2, end2);
	std::copy_n(begin1, std::min(l, r), begin2);
	return l - r;
}

template <typename T, typename FN, typename F0, typename FP>
void arif(T value, FN negative, F0 zero, FP positive)
{
	if(value > 0)
	{
		positive();
		return;
	}
	if(value < 0)
	{
		negative();
		return;
	}
	zero();
}
