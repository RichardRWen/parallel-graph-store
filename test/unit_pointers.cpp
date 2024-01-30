#include <iostream>
#include <cstdlib> // srand, rand

//#include <parlay/parallel.h>

#include "../include/pointer.h"

#define PTR_SIZE 256

#define Ptr Pointer<PTR_SIZE>

int main(int argc, char **argv) {
	srand(time(NULL));

	SharedMemory mem;
	SharedGraph graph(&mem, 10);

	for (int i = 0; i < graph.vertices.size(); i++) {
		for (int j = 0; j < graph.vertices.size() - 1; j++) {
			if (rand() % 2 == 0) {
				graph[i].push_back(j < i ? j : j + 1);
			}
		}
	}

	PointerManager mgr(0, &mem, &graph);

	size_t vid = 4;

	mgr.start_op();
	Ptr old = graph[vid];
	Ptr ptr = mgr.borrow();
	// do some stuff with ptr1
	while (!mgr.try_swap(vid, old, ptr)) {
		mgr.start_op();
		old = graph[vid];
		// conflict resolution
	}
	mgr.restore(old);
	
	mgr.try_free();

	return 0;
}
