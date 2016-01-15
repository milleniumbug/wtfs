#include <iostream>
#include <unordered_map>
#include <utility>
#include <tuple>

typedef std::tuple<int, int, std::string*> Iterator;

int main()
{
	std::unordered_map<int, std::pair<int, std::string>> m = {
		{15, { 0^45, "frist" } },
		{45, { 15^20, "scond" } },
		{20, { 45^100, "trzeci" } },
		{100, { 20^77, "czwarty" } },
		{77, { 100^0, "funf" } },
	};
	auto newit_start = [&](int x){
		return Iterator(0, x, &m.at(x).second);
	};
	auto newit_end = [&](int x){
		return Iterator(x, 0, &m.at(x).second);
	};
	auto has_next_entry = [&](Iterator& it) {
		return std::get<1>(it) != 0;
	};
	auto has_prev_entry = [&](Iterator& it) {
		return std::get<0>(it) != 0;
	};
	auto next_entry = [&](Iterator& it){
		Iterator newit;
		constexpr size_t value = 2;
		constexpr size_t curr = 1;
		constexpr size_t prev = 0;
		std::get<curr>(newit) = std::get<prev>(it) ^ m.at(std::get<curr>(it)).first;
		std::get<prev>(newit) = std::get<curr>(it);
		std::get<value>(newit) = std::get<curr>(newit) != 0 ? &m.at(std::get<curr>(newit)).second : &m.at(std::get<prev>(newit)).second;
		it = newit;
	};
	auto prev_entry = [&](Iterator& it){
		Iterator newit;
		constexpr size_t value = 2;
		constexpr size_t curr = 0;
		constexpr size_t prev = 1;
		std::get<curr>(newit) = std::get<prev>(it) ^ m.at(std::get<curr>(it)).first;
		std::get<prev>(newit) = std::get<curr>(it);
		std::get<value>(newit) = std::get<curr>(newit) != 0 ? &m.at(std::get<curr>(newit)).second : &m.at(std::get<prev>(newit)).second;
		it = newit;
	};
	auto deref = [&](Iterator& p) -> auto& {
		return *std::get<2>(p);
	};
	auto debug_print = [&](auto&& x){
		std::cout << deref(x) << "\n";
	};
	{
		for(Iterator it = newit_start(15); has_next_entry(it); next_entry(it))
		{
			debug_print(it);
		}
		for(Iterator it = newit_end(77); has_prev_entry(it); prev_entry(it))
		{
			debug_print(it);
		}
	}
	{
		Iterator it = newit_start(15);
		for(; has_next_entry(it); next_entry(it))
		{
			debug_print(it);
		}
		for(; has_prev_entry(it); prev_entry(it))
		{
			debug_print(it);
		}
	}
}