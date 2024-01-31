#ifndef _POINTER_H_
#define _POINTER_H_

#include <cstdint>
#include <mutex>
#include <atomic>
#include <ctime>
#include <fstream>
#include <deque>

#include <parlay/alloc.h>


template <typename T> class Graph;

class GlobalManager {
public:
	std::vector<std::time_t> start_times;

	unsigned char *malloc(size_t size) {
		return (unsigned char*)parlay::p_malloc(size);
	}
};

// A pointer wrapper storing the address of an array.
// The max capacity of the array should be fixed at initialization and known by the calling party.
// The current size of the array should be stored in the first 16 bits.
template <typename T>
struct Pointer {
	// TODO: consider evaluating the overhead of also storing the capacity next to the size
	unsigned char *data;

	Pointer() : data(NULL) {}
	Pointer(GlobalManager *mgr, const uint16_t capacity) : data(mgr->malloc(capacity * sizeof(T) + sizeof(uint16_t))) {
		*((uint16_t*)data) = 0;
	}
	Pointer(unsigned char *_data) : data(_data) {}

	uint16_t size() {
		return *((uint16_t*)data);
	}

	T operator [] (uint16_t i) {
		return ((T*)(data + sizeof(uint16_t)))[i];
	}

	void push_back(T val) { // TODO: maybe want to consider checking fullness first? would induce some overhead
		data[(*((uint16_t*)data))++] = val;
	}
};


template <typename T>
class PointerManager {
	uint32_t id;
	Graph<T> *graph;
	GlobalManager *global_mgr;
	std::deque<Pointer<T>> del; // not thread-safe but in proper operation shouldn't be called in parallel
	std::deque<std::time_t> del_times;
	
public:
	PointerManager(uint32_t _id, GlobalManager *_global_mgr, Graph<T> *_graph) : id(_id), graph(_graph), global_mgr(_global_mgr) {}

	void start_op() {
		global_mgr->start_times[id] = time(NULL);
	}

	bool try_free() {
		int num_freed = 0;
		while (del.size() > 0) {
			// this for-loop could be replaced with parlay::any_of if the number of threads is high enough
			for (std::time_t start_time : global_mgr->start_times) {
				if (start_time <= del_times.front()) {
					return num_freed;
				}
			}
			parlay::p_free(del.front().data);
			del.pop_front();
			del_times.pop_front();
			num_freed++;
		}
		return num_freed;
	}

	void mark_del(Pointer<T> ptr) {
		del.push_back(ptr);
		del_times.push_back(time(NULL));
	}

	Pointer<T> operator [] (size_t i) {
		return graph[i];
	}

	bool try_swap(size_t vertex_id, Pointer<T> exp_ptr, Pointer<T> upd_ptr) {
		return graph->vertices[vertex_id].compare_exchange_strong(exp_ptr, upd_ptr);
	}
};

#endif
