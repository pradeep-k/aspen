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

    size_t updates_to_run = batch_size;
    auto updates = pbbs::sequence<pair_vertex>(updates_to_run);
    auto deletes = pbbs::sequence<pair_vertex>(updates_to_run);
    int updates_count = 0;
	int deletes_count = 0;
	size_t nn = 1 << (pbbs::log2_up(n));
    cout << "nn for rmat " << nn << endl;

    FILE* file = fopen(update_fname.c_str(), "rb");
    edge_t edge;

    cout << "Inserting" << endl;
    int offset = 0;
    for (int j = 0; j < loop_count; j++) {
        timer st; 
        st.start();
        // get data from file
        for (auto i = 0; i < batch_size; ++i) {
            if (get_next_text_line(file, edge)) {
				if (edge.src < 0) {//deletion case
					deletes[i] = {-edge.src, edge.dst};
					++deletes_count;
				} else {
					updates[i] = {edge.src, edge.dst};
					++updates_count;
				}
            } else {
                break;
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
        offset += batch_size;
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
