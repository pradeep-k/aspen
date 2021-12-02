#include "../graph/api.h"
#include "../trees/utils.h"
#include "../lib_extensions/sparse_table_hash.h"
#include "../pbbslib/random_shuffle.h"

#include <cstring>

#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>


#include <iostream>
#include <fstream>

#include "rmat_util.h"

using namespace std;
using pair_vertex = tuple<uintV, uintV>;
    
struct edge_t {
    int src;
    int dst;
};


int64_t fsize(const string& fname)
{
    struct stat st; 
    if (0 == stat(fname.c_str(), &st)) {
        return st.st_size;
    }   
    perror("stat issue");
	assert(0);
    return 0;
}


bool get_next_binary_line(FILE* file, edge_t& edge)
{
    if((fread(&edge, sizeof(edge), 1, file)) > 0) {
        return true;
    }
    return false;
}

bool get_next_text_line(FILE* file, edge_t& edge)
{
    char line[128];
	char* line1;
    const char* del = " \t\n";
    char* token = 0;
	if (fgets(line, sizeof(line), file)) {
		line1 = line;
		token = strtok_r(line1, del, &line1);
		sscanf(token, "%d", &edge.src);
		token = strtok_r(line1, del, &line1);
		sscanf(token, "%d", &edge.dst);
		return true;
	}
    return false;
}

void parallel_updates(commandLine& P) {
    string update_fname = P.getOptionValue("-update-file", "updates.dat");
    size_t batch_size = P.getOptionLongValue("-batch-size", 65536);
    size_t loop_count = P.getOptionLongValue("-batch-count", 64);
    auto update_sizes =  batch_size*loop_count;// 1000000000;

    auto VG = initialize_treeplus_graph(P);

    cout << "calling acquire_version" << endl;
    versioned_graph<treeplus_graph>::version S = VG.acquire_version();
    cout << "acquired!" << endl;
    const auto& GA = S.graph;
    size_t n = GA.num_vertices();
    size_t E = GA.num_edges();
    cout << "V and E are " << n  << " "<< E << endl;
    //VG.release_version(std::move(S));


    //auto  S1 = pbbs::sequence<versioned_graph<treeplus_graph>::version>(10); 
    //versioned_graph<treeplus_graph>::version* S1 = (versioned_graph<treeplus_graph>::version*)calloc(sizeof(versioned_graph<treeplus_graph>::version), loop_count); 

    double avg_insert = 0.0;
    double avg_delete = 0.0;

    size_t updates_to_run = update_sizes;
    auto updates = pbbs::sequence<pair_vertex>(batch_size);
    auto deletes = pbbs::sequence<pair_vertex>(batch_size);
    int updates_count = 0;
	int deletes_count = 0;
	size_t nn = 1 << (pbbs::log2_up(n));
    cout << "nn for rmat " << nn << endl;

    FILE* file = fopen(update_fname.c_str(), "rb");
    int64_t file_size = fsize(update_fname.c_str());
	edge_t* edges = (edge_t*) malloc(file_size) ;
    edge_t edge;
    
	size_t edge_count = 0;
	while (get_next_text_line(file, edge)) {
		edges[edge_count++] = edge;
	}

    cout << "Inserting" << endl;
    int offset = 0;
    size_t min_batch = batch_size;
     
	while (edge_count != 0) {
		timer st; 
        st.start();
		min_batch = std::min(batch_size, edge_count);
		edge_count -= min_batch;
        // get data from file
        for (auto i = 0; i < min_batch; ++i) {
            if (edges[offset+i].src < 0) {//deletion case
                deletes[i] = {-edges[offset+i].src, edges[offset+i].dst};
                ++deletes_count;
            } else {
                updates[i] = {edges[offset+i].src, edges[offset+i].dst};
                ++updates_count;
            }
        }
        //insert it
        VG.insert_edges_batch(updates_count, updates.begin(), false, true, nn, false);
        if (deletes_count != 0) {
            VG.delete_edges_batch(deletes_count, deletes.begin(), false, true, nn, false);
        }
		double batch_time = st.stop();

        cout << "batch time = " << batch_time << endl << endl;
        avg_insert += batch_time;
        offset += min_batch;
		updates_count = 0;
		deletes_count = 0;
    }

    //S1[start] = VG.acquire_version();
    // cout << "Finished bs: " << update_sizes[us] << endl;
    cout << "Total insert time: " << (avg_insert) << endl;
    //cout << "Avg delete: " << (avg_delete) << endl << endl;
    auto snap = VG.acquire_version();
    const auto& GA1 = snap.graph;
    size_t E_total = GA1.num_edges();
    cout << " E = " << E_total << endl << endl;
}

int main(int argc, char** argv) {
  cout << "Running with " << num_workers() << " threads" << endl;
  commandLine P(argc, argv, "./test_graph [-f file -m (mmap) <testid>]");

  parallel_updates(P);
}
