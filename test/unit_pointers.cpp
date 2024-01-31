#include <iostream>
#include <cstdlib> // srand, rand

//#include <parlay/parallel.h>

#include "../include/pointer.h"
#include "../include/graph.h"


int main(int argc, char **argv) {
	srand(time(NULL));

	using id_type = uint32_t;

	GlobalManager mem;
	Graph<id_type> graph(&mem, 32);

	for (int i = 0; i < graph.vertices.size(); i++) {
		for (int j = 0; j < graph.vertices.size() - 1; j++) {
			if (rand() % 2 == 0) {
				graph[i].push_back(j < i ? j : j + 1);
			}
		}
	}

	std::vector<PointerManager<id_type>> mgrs;
	for (int i = 0; i < 5; i++) {
		mgrs.emplace_back(0, &mem, &graph);
	}

	size_t vid = 4;

	mgrs[0].start_op();
	Pointer<id_type> old = graph[vid];
	Pointer<id_type> ptr(&mem, graph.max_degree);
	// do some stuff with ptr1
	while (!mgrs[0].try_swap(vid, old, ptr)) {
		mgrs[0].start_op();
		old = graph[vid];
		// conflict resolution
	}
	mgrs[0].mark_del(old);
	
	mgrs[0].try_free();

	return 0;
}
