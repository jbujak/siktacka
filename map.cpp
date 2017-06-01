#include <set>
#include <utility>

#include "map.h"

extern "C" {
	static std::set< std::pair<int, int> > map;

	void map_new() {
		map.clear();
	}

	void map_insert(int x, int y) {
		map.insert(std::make_pair(x, y));
	}

	bool map_contains(int x, int y) {
		return map.find(std::make_pair(x, y)) != map.end();
	}
}
