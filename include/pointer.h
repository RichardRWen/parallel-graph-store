#include <cstdint>
#include <mutex>
#include <atomic>
#include <ctime>
#include <fstream>

template <size_t ptr_size = 256>
struct Pointer {
	static const size_t capacity = ptr_size;
	uint16_t size; // Could this datatype be chosen at compile time? Eg. use a char[] with length based on ptr_size
	unsigned char *data;

	Pointer() : size(0), data(NULL) {}
	Pointer(unsigned char *_data) : size(0), data(_data) {}
}

template <size_t ptr_size>
class PointerManager {
	SharedGraph<ptr_size> *shared_graph;
	SharedMemory<ptr_size> *shared_mem;
	std::deque<Pointer<ptr_size>> free;
	std::deque<Pointer<ptr_size>> del;
	std::deque<std::time_t> del_times;
	
	PointerManager(SharedGraph<ptr_size> *_shared_graph, SharedMemory *_shared_mem, int num_ptrs) : shared_graph(_shared_graph), shared_mem(_shared_mem) {
		for (int i = 0; i < num_ptrs; i++) {
			Pointer ptr(shared_mem.alloc());
			free.push_front(ptr);
		}
	}

	void try_free() {
		while (true) {
			for (std::time_t start_time : shared_mem.start_times) {
				if (start_time <= del_times.front) {
					return;
				}
			}
			free.push_front(del.pop_front);
			del_times.pop_front;
		}
	}

	Pointer borrow() {
		if (free.empty()) {
			try_free(); // Maybe we shouldn't do this, and just go straight to allocating
			if (free.empty()) {
				Pointer ret(shared_mem.alloc());
				return ret;
			}
		}
		return free.pop_front();
	}

	void return(Pointer ptr) { // Might have to worry about race conditions on insert/resize
		del.push_back(ptr);
		del_times.push_back(time(NULL));
	}

	Pointer operator [] (size_t i) {
		return shared_graph->vertices[i].load(std::memory_order_relaxed);
	}

	bool try_swap(size_t vid, Pointer exp_ptr, Pointer upd_ptr) {
		return shared_graph->vertices[i].compare_exchange_strong(exp_ptr, upd_ptr);
	}
}

template <size_t _ptr_size = 256, size_t block_size = 100>
class SharedMemory {
public:
	static const size_t ptr_size = _ptr_size;
	std::vector<unsigned char*> blocks;
	std::mutex blocks_lock;
	size_t capacity = 0;
	std::atomic<size_t> size(0);

	std::vector<std::time_t> start_times;

	unsigned char *alloc() {
		while (true) {
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
		}
	}
}

template <size_t ptr_size>
class SharedGraph {
public:
	SharedMemory<ptr_size> *shared_mem;
	std::vector<std::atomic<Pointer<ptr_size>>> vertices;
	
	SharedGraph(SharedMemory<ptr_size> *_shared_mem, size_t size) : shared_mem(_shared_mem) {
		for (size_t i = 0; i < size; i++) {
			vertices.emplace_back();
		}
	}

	void resize(size_t new_size) { // don't currently support downsizing
		if (new_size > vertices.size()) {
			for (size_t size = vertices.size(); size < new_size; size++) {
				vertices.emplace_back();
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
		size_reader.read((unsigned char*)(&num_vertices), sizeof(uint64_t));
		size_reader.read((unsigned char*)(&num_bytes), sizeof(uint64_t));
		edge_reader.seekg(2 * sizeof(uint64_t) + num_bytes);
		resize(num_vertices);

		uint16_t ptr_size;
		for (uint64_t i = 0; i < num_vertices; i++) {
			size_reader.read((unsigned char*)(&ptr_size), sizeof(uint16_t));
			edge_reader.read((unsigned char*)vertices[i].data, ptr_size);
		}

		size_reader.close();
		edge_reader.close();
	}

	bool save(std::string filename) { // Might worry about others editing the graph during this - mark want_to_save flag and wait for semaphore?
		std::ofstream writer(filename);
		if (!writer.is_open()) return false;

		uint64_t num_vertices = vertices.size();
		writer.write((unsigned char*)(&num_vertices), sizeof(uint64_t));

		uint64_t num_bytes = 0;
		for (std::atomic<Pointer<ptr_size>> edge_list : vertices) {
			num_bytes += edge_list.load(std::memory_order_relaxed).size;
		}
		writer.write((unsigned char*)(&num_bytes), sizeof(uint64_t));

		for (std::atomic<Pointer<ptr_size>> edge_list : vertices) {
			writer.write((unsigned char*)&edge_list.load(std::memory_order_relaxed).size, sizeof(uint16_t));
		}
		for (std::atomic<Pointer<ptr_size>> edge_list : vertices) {
			Pointer<ptr_size> edges = edge_list.load(std::memory_order_relaxed);
			writer.write(edges.data, edges.size);
		}
		
		writer.close();
		return true;
	}
}
