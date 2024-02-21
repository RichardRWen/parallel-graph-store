#ifndef _GRAPH_H_
#define _GRAPH_H_


class GlobalManager;
template <typename T> struct Pointer;

template <typename T> // TODO: make more clear that this type is the index type stored in the edge lists; point also uses T but with a different meaning
class Graph {
public:
	GlobalManager *global_mgr;
	std::deque<std::atomic<Pointer<T>>> vertices; // TODO: ensure these atomics are lock-free
	uint64_t start = 0;
	const uint16_t max_degree;
	
	Graph(GlobalManager *_global_mgr, uint16_t _max_degree) : global_mgr(_global_mgr), vertices(0), max_degree(_max_degree) {}

	Pointer<T> operator [] (size_t i) {
		return vertices[i].load(std::memory_order_relaxed);
	}

	void resize(size_t new_size) { // don't currently support downsizing; also not thread-safe
		if (new_size > vertices.size()) {
			for (size_t i = vertices.size(); i < new_size; i++) {
				vertices.emplace_back(global_mgr, max_degree);
			}
		}
	}

	void push_back(unsigned char *ptr) {
		vertices.emplace_back(ptr);
	}
	void emplace_back() {
		vertices.emplace_back(global_mgr, max_degree);
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
		// TODO: clean this up by making the list enumerable?
		// TODO: ensure these foreach loops aren't copying atomics unnecessarily
		for (std::atomic<Pointer<T>> edge_list : vertices) {
			num_bytes += edge_list.load(std::memory_order_relaxed).size;
		}
		writer.write((char*)(&num_bytes), sizeof(uint64_t));

		for (std::atomic<Pointer<T>> edge_list : vertices) {
			writer.write((char*)(&edge_list.load(std::memory_order_relaxed).size), sizeof(uint16_t));
		}
		for (std::atomic<Pointer<T>> edge_list : vertices) {
			Pointer<T> edges = edge_list.load(std::memory_order_relaxed);
			writer.write(edges.data, edges.size);
		}
		
		writer.close();
		return true;
	}
};

#endif
