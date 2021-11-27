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
using edge_seq = pair<uintV, uintV>;

void parallel_updates(commandLine& P) {
  string update_fname = P.getOptionValue("-update-file", "updates.dat");

  auto VG = initialize_treeplus_graph(P);

  cout << "calling acquire_version" << endl;
  versioned_graph<treeplus_graph>::version S = VG.acquire_version();
  cout << "acquired!" << endl;
  const auto& GA = S.graph;
  size_t n = GA.num_vertices();
  size_t E = GA.num_edges();
  cout << "V and E are " << n  << " "<< E << endl;
  //VG.release_version(std::move(S));

  using pair_vertex = tuple<uintV, uintV>;

  auto r = pbbs::random();
  // 2. Generate the sequence of insertions and deletions

  auto update_sizes = pbbs::sequence<size_t>(1);

  size_t batch_size = 65536;
  size_t loop_count = 32;
  update_sizes[0] =  batch_size*loop_count;// 1000000000;
  
  //auto  S1 = pbbs::sequence<versioned_graph<treeplus_graph>::version>(10); 
  versioned_graph<treeplus_graph>::version* S1 = (versioned_graph<treeplus_graph>::version*)calloc(sizeof(versioned_graph<treeplus_graph>::version), loop_count); 
  
  
  auto update_times = std::vector<double>();
  size_t n_trials = 3;

  size_t start = 0;
  for (size_t us=start; us<update_sizes.size(); us++) {
    double avg_insert = 0.0;
    double avg_delete = 0.0;
    cout << "Running bs: " << update_sizes[us] << endl;

    size_t updates_to_run = update_sizes[us];
    auto updates = pbbs::sequence<pair_vertex>(updates_to_run);

    double a = 0.5;
    double b = 0.1;
    double c = 0.1;
    size_t nn = 1 << (pbbs::log2_up(n));
    cout << "nn for rmat " << nn << endl;
    auto rmat = rMat<uintV>(nn, r.ith_rand(0), a, b, c);

    parallel_for(0, updates.size(), [&] (size_t i) {
        updates[i] = rmat(i);
    });
    //--end of graph edge data 

    int offset = 0;
    for (int i = 0; i < loop_count; i++)
    {
        cout << "Inserting" << endl;
        timer st; st.start();
        VG.insert_edges_batch(batch_size, updates.at(offset), false, true, nn, false);
        double batch_time = st.stop();

        cout << "batch time = " << batch_time << endl;
        avg_insert += batch_time;
        offset += batch_size;
        /*
        S1[i] = VG.acquire_version();
        const auto& GA = S1[i].graph;
        size_t E_total = GA.num_edges();
        cout << " E = " << E_total << endl << endl;
        if (i > 1) {
            VG.release_version(std::move(S1[i - 1]));
         }*/
    }
      /*
      {
        cout << "Deleting" << endl;
        timer st; st.start();
        VG.delete_edges_batch(update_sizes[us], updates.begin(), false, true, nn, false);
        double batch_time = st.stop();

        cout << "batch time = " << batch_time << endl << endl;
        avg_delete += batch_time;
      }
      */

    //S1[start] = VG.acquire_version();
    // cout << "Finished bs: " << update_sizes[us] << endl;
    cout << "Total insert time: " << (avg_insert) << endl;
    //cout << "Avg delete: " << (avg_delete / n_trials) << endl << endl;
    auto snap = VG.acquire_version();
    const auto& GA = snap.graph;
    size_t E_total = GA.num_edges();
    cout << " E = " << E_total << endl << endl;
  }
}

int main(int argc, char** argv) {
  cout << "Running with " << num_workers() << " threads" << endl;
  commandLine P(argc, argv, "./test_graph [-f file -m (mmap) <testid>]");

  parallel_updates(P);
}
