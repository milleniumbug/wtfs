#include "common.hpp"
#include "structure.hpp"
#include "tests.hpp"

namespace tests
{
template <typename Tester>
void tests(Tester test, wtfs& fs)
{
	auto resolve_result_tied = [](const resolve_result& r)
	{
		return std::tie(
		    r.parents, r.base, r.successfully_resolved, r.failed_to_resolve);
	};
	test("resolve_dirs root", [&]()
	    {
		    resolve_result actual = resolve_dirs("/", fs);
		    resolve_result expected;
		    expected.parents = {std::make_pair("root", &fs.root)};
		    expected.base = boost::none;
		    expected.successfully_resolved = 1;
		    expected.failed_to_resolve = 0;
		    return resolve_result_tied(actual) == resolve_result_tied(expected);
		});
	test("resolve_dirs noexist", [&]()
	    {
		    resolve_result actual = resolve_dirs("/noexist", fs);
		    resolve_result expected;
		    expected.parents = {std::make_pair("root", &fs.root)};
		    expected.base = boost::none;
		    expected.successfully_resolved = 1;
		    expected.failed_to_resolve = 1;
		    return resolve_result_tied(actual) == resolve_result_tied(expected);
		});
	test("resolve_dirs directory", [&]()
	    {
		    resolve_result actual = resolve_dirs("/asdf", fs);
		    resolve_result actual2 = resolve_dirs("/asdf/", fs);
		    resolve_result expected;
		    expected.parents = {std::make_pair("root", &fs.root)};
		    expected.base =
		        std::make_pair("asdf", &fs.root.lookup_directory("asdf").get());
		    expected.successfully_resolved = 2;
		    expected.failed_to_resolve = 0;
		    return resolve_result_tied(actual) ==
		               resolve_result_tied(expected) &&
		           resolve_result_tied(actual2) ==
		               resolve_result_tied(expected);
		});
	test("resolve_dirs longer path", [&]()
	    {
		    resolve_result actual = resolve_dirs("/asdf/koles", fs);
		    resolve_result expected;
		    expected.parents = {std::make_pair("root", &fs.root),
		        std::make_pair("asdf",
		                            &fs.root.lookup_directory("asdf").get())};
		    expected.base = std::make_pair("koles", 2);
		    expected.successfully_resolved = 3;
		    expected.failed_to_resolve = 0;
		    return resolve_result_tied(actual) == resolve_result_tied(expected);
		});
	test("resolve_dirs longer path noexist", [&]()
	    {
		    resolve_result actual = resolve_dirs("/asdf/noexist", fs);
		    resolve_result expected;
		    expected.parents = {std::make_pair("root", &fs.root),
		        std::make_pair("asdf",
		                            &fs.root.lookup_directory("asdf").get())};
		    expected.base = boost::none;
		    expected.successfully_resolved = 2;
		    expected.failed_to_resolve = 1;
		    return resolve_result_tied(actual) == resolve_result_tied(expected);
		});
	test("resolve_dirs longer path file as directory", [&]()
	    {
		    resolve_result actual = resolve_dirs("/asdf/koles/noexist", fs);
		    resolve_result expected;
		    expected.parents = {std::make_pair("root", &fs.root),
		        std::make_pair("asdf",
		                            &fs.root.lookup_directory("asdf").get())};
		    expected.base = boost::none;
		    expected.successfully_resolved = 2;
		    expected.failed_to_resolve = 2;
		    return resolve_result_tied(actual) == resolve_result_tied(expected);
		});
}
}

void run_tests(wtfs& fs)
{
	int counter = 0;
	auto test = [&](auto&& name, auto&& fun)
	{
		++counter;
		bool result = fun();
		std::cout << counter << ". " << name << ": "
		          << (result ? "PASS\n" : "FAIL\n");
		std::cout.flush();
	};
	tests::tests(test, fs);
}