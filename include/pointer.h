#include <cstdint>
#include <mutex>
#include <atomic>
#include <ctime>
#include <fstream>
#include <deque>

#include <parlay/alloc.h>

#include "include/graph.h"

template <size_t ptr_size = 256>
struct Pointer {
	static const size_t capacity = ptr_size;
	uint16_t size; // Could this datatype be chosen at compile time? Eg. use a char[] with length based on ptr_size
	unsigned char *data;

	Pointer() : size(0), data(NULL) {}
	Pointer(unsigned char *_data) : size(0), data(_data) {}

	template <typename val_type = uint32_t>
	void push_back(val_type val) {
		if (size >= ptr_size) return;
		*(data + size) = val;
		size += sizeof(val_type);
	}
};

template <size_t _ptr_size = 256, size_t block_size = 100>
class SharedMemory {
public:
	static const size_t ptr_size = _ptr_size;
	std::vector<unsigned char*> blocks;
	std::mutex blocks_lock;
	size_t capacity = 0;
	std::atomic<size_t> size;

	std::vector<std::time_t> start_times;

	SharedMemory() : size(0) {}

	unsigned char *alloc() {
		/*while (true) {
			size_t size_exp = size.load(std::memory_order_relaxed);
			if (size_exp >= blocks.size() * block_size) {
				blocks_lock.lock();
				if (size_exp >= capacity) {
					blocks.push_back(new unsigned char[ptr_size * block_size]);
					capacity += block_size;
				}
				blocks_lock.unlock();
			}
			unsigned char *ret = &(blocks[blocks.size() - 1][size - block_size * blocks.size()]);
			if (size.compare_exchange_strong(size_exp, size_exp + 1)) return ret;
		}*/
		return (unsigned char*)parlay::p_malloc(ptr_size);
	}
};

template <size_t ptr_size>
class SharedGraph {
public:
	SharedMemory<ptr_size> *shared_mem;
	std::deque<std::atomic<Pointer<ptr_size>>> vertices;
	uint64_t start = 0;
	
	SharedGraph(SharedMemory<ptr_size> *_shared_mem, size_t size) : vertices(size), shared_mem(_shared_mem) {
		for (size_t i = 0; i < size; i++) {
			vertices.emplace_back(shared_mem->alloc());
		}
	}

	Pointer<ptr_size> operator [] (size_t i) {
		return vertices[i].load(std::memory_order_relaxed);
	}

	void resize(size_t new_size) { // don't currently support downsizing; also not thread-safe
		if (new_size > vertices.size()) {
			for (size_t i = vertices.size(); i < new_size; i++) {
				vertices.emplace_back(shared_mem->alloc());
				//vertices[i].data = shared_mem->alloc();
			}
		}
	}

	void push_back(unsigned char *ptr) {
		vertices.emplace_back(ptr);
	}

	bool load(std::string filename) {
		std::ifstream size_reader(filename);
		std::ifstream edge_reader(filename);
		if (!size_reader.is_open() || !edge_reader.is_open()) return false;

		uint64_t num_vertices, num_bytes;
		size_reader.read((char*)(&num_vertices), sizeof(uint64_t));
		size_reader.read((char*)(&num_bytes), sizeof(uint64_t));
		edge_reader.seekg(2 * sizeof(uint64_t) + num_bytes);
		resize(num_vertices);

		uint16_t edge_list_len;
		for (uint64_t i = 0; i < num_vertices; i++) {
			size_reader.read((char*)(&edge_list_len), sizeof(uint16_t));
			edge_reader.read((char*)vertices[i].data, edge_list_len);
		}

		size_reader.close();
		edge_reader.close();
	}

	bool save(std::string filename) { // Might worry about others editing the graph during this - mark want_to_save flag and wait for semaphore?
		std::ofstream writer(filename);
		if (!writer.is_open()) return false;

		uint64_t num_vertices = vertices.size();
		writer.write((char*)(&num_vertices), sizeof(uint64_t));

		uint64_t num_bytes = 0;
		for (std::atomic<Pointer<ptr_size>> edge_list : vertices) {
			num_bytes += edge_list.load(std::memory_order_relaxed).size;
		}
		writer.write((char*)(&num_bytes), sizeof(uint64_t));

		for (std::atomic<Pointer<ptr_size>> edge_list : vertices) {
			writer.write((char*)(&edge_list.load(std::memory_order_relaxed).size), sizeof(uint16_t));
		}
		for (std::atomic<Pointer<ptr_size>> edge_list : vertices) {
			Pointer<ptr_size> edges = edge_list.load(std::memory_order_relaxed);
			writer.write(edges.data, edges.size);
		}
		
		writer.close();
		return true;
	}
};


template <size_t ptr_size = 256>
class PointerManager {
	uint32_t id;
	SharedGraph<ptr_size> *graph;
	SharedMemory<ptr_size> *shared_mem;
	//std::deque<Pointer<ptr_size>> free;
	std::deque<Pointer<ptr_size>> del;
	std::deque<std::time_t> del_times;
	std::mutex del_lock;
	
public:
	PointerManager(uint32_t _id, SharedMemory<ptr_size> *_shared_mem, SharedGraph<ptr_size> *_graph, int num_ptrs = 0) : id(_id), shared_graph(_shared_graph), shared_mem(_shared_mem) {
		/*for (int i = 0; i < num_ptrs; i++) {
			Pointer ptr(shared_mem.alloc());
			free.push_front(ptr);
		}*/
	}

	void start_op() {
		shared_mem->start_times[id] = time(NULL);
	}

	void try_free() {
		del_lock.lock();
		while (true) {
			for (std::time_t start_time : shared_mem.start_times) {
				if (start_time <= del_times.front()) {
					del_lock.unlock();
					return;
				}
			}
			free.push_front(del.pop_front());
			p_free(del.front().data);
			del.pop_front();
			del_times.pop_front();
		}
		del_lock.unlock();
	}

	Pointer<ptr_size> borrow() {
		/*if (free.empty()) {
			try_free(); // Maybe we shouldn't do this, and just go straight to allocating
			if (free.empty()) {
				Pointer ret(shared_mem.alloc());
				return ret;
			}
		}
		return free.pop_front();*/
		Pointer ret(shared_mem.alloc());
		return ret;
	}

	void restore(Pointer<ptr_size> ptr) {
		del_lock.lock();
		del.push_back(ptr);
		del_times.push_back(time(NULL));
		del_lock.unlock();
	}

	Pointer<ptr_size> operator [] (size_t i) {
		return shared_graph->vertices[i].load(std::memory_order_relaxed);
	}

	bool try_swap(size_t vertex_id, Pointer<ptr_size> exp_ptr, Pointer<ptr_size> upd_ptr) {
		return shared_graph->vertices[vertex_id].compare_exchange_strong(exp_ptr, upd_ptr);
	}
};
