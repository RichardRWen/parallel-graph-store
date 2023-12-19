#include <iostream>
#include <cstdlib> // srand, rand

//#include <parlay/parallel.h>

#include "../include/pointer.h"

int main(int argc, char **argv) {
	srand(time(NULL));

	SharedMemory mem;
	SharedGraph graph(&mem, 10);

	for (int i = 0; i < graph.vertices.size(); i++) {
		for (int j = 0; j < graph.vertices.size() - 1; j++) {
			if (rand() % 2 == 0) {
				graph.vertices[i].load(std::memory_order_relaxed).push_back(j < i ? j : j + 1);
			}
		}
	}

	PointerManager mgr0(0, &mem, &graph);

	return 0;
}
