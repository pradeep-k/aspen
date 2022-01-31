#include <algorithm>

#include "new_type.h"
#include "batching.h"
#include "util.h"
#include "graph_view.h"
#include "diff_bfs.h"

using std::move;

using pair_vertex = tuple<uintV, uintV>;

typekv_t* typekv;

versioned_graph<treeplus_graph> VG;
list_head snapshot_list;

int _part_count;
index_t residue = 0;
int THD_COUNT = 9;
vid_t _global_vcount = 0;
int _dir = 0;//undirected
int _source = 0;//text
int _num_sources = 1;//only one data-source
int _format = 0;
int64_t _arg = 1;//algo specific argument
int _numtasks = 0, _rank = 0;
int _numlogs = 1;

index_t  BATCH_SIZE = (1L << 16);//edge batching in edge log
index_t  BATCH_MASK =  0xFFFF;
index_t  BATCH_TIMEOUT = (-1L);

#ifdef B64
propid_t INVALID_PID = 0xFFFF;
tid_t    INVALID_TID  = 0xFFFFFFFF;
sid_t    INVALID_SID  = 0xFFFFFFFFFFFFFFFF;
#else 
propid_t INVALID_PID = 0xFF;
tid_t    INVALID_TID  = 0xFF;
sid_t    INVALID_SID  = 0xFFFFFFFF;
#endif

//auto updates = pbbs::sequence<pair_vertex>(2*BATCH_SIZE);
//auto deletes = pbbs::sequence<pair_vertex>(2*BATCH_SIZE);

pair_vertex* updates;
pair_vertex* deletes;

//run adjacency store creation here.
status_t create_adjacency_snapshot(ubatch_t* ubatch)
{
    double start =  mywtime();
    status_t status  = ubatch->create_mbatch();
    if (0 == ubatch->reader_archive) {
        assert(0);
    }

    vsnapshot_t* startv = ubatch->get_archived_vsnapshot();

    if (startv) {
        startv = startv->get_prev();
    } else {
        startv = ubatch->get_oldest_vsnapshot();
    }

    vsnapshot_t* endv   = ubatch->get_to_vsnapshot();
    blog_t* blog = ubatch->blog;
    
    int updates_count = 0;
	int deletes_count = 0;
    vid_t src, dst;
    index_t tail, marker, index;
    edge_t* edge, *edges;
    vid_t v_count = typekv->get_type_vcount(0);
    do {
        edges = blog->blog_beg;
        tail = startv->tail;
        marker = startv->marker;
        for (index_t i = tail; i < marker; ++i) {
            index = (i & blog->blog_mask);
            edge = (edge_t*)((char*)edges + index*ubatch->edge_size);
            src = edge->src_id;
            dst = TO_SID(edge->get_dst());
            
            assert(TO_SID(src) < v_count);
            assert(dst < v_count);
            if (IS_DEL(src)) {//deletion case
                deletes[deletes_count] = {TO_SID(src), dst};
                ++deletes_count;
                deletes[deletes_count] = {dst, TO_SID(src)};
                ++deletes_count;
            } else {
                updates[updates_count] = {src, dst};
                ++updates_count;
                updates[updates_count] = {dst, src};
                ++updates_count;
            }
        }

        //insert it
        VG.insert_edges_batch(updates_count, updates, false, true, v_count, false);
        if (deletes_count != 0) {
			VG.delete_edges_batch(deletes_count, deletes, false, true, v_count, false);
        }
    } while (startv != endv);


    //create snasphot here, and add it to the linked list. So that analytics can use it.
    //versioned_graph<treeplus_graph>::version S2 = VG.acquire_version(); 
    adj_snap_t* S = (adj_snap_t*)malloc(sizeof(adj_snap_t));
    S->S = VG.acquire_version1();
    S->snap_id = endv->id;
    S->ref_count = 0;
    
    list_add(&S->list, &snapshot_list);
    
    
    auto it = (adj_snap_t*)snapshot_list.get_prev();//.begin()
    if (endv->id == 1) {
        it->ref_count = 1;//first snapshot is special
    } else { 
        //S->ref_count = 2;
        while(it->ref_count == 0) {
            //cout << "deleting " << it->snap_id << endl;
            list_del(&it->list);
            VG.release_version1(it->S);
            delete(it->S);
            free(it);
            it = (adj_snap_t*)snapshot_list.get_prev();
        }
    }
    
    //updating is required
    ubatch->update_marker();
    //cout << endv->id << endl << endl;
    double end = mywtime();
    cout << endv->id << ":" << end - start << endl;


    //if analytics is not running, let us remove it. 
    /*if (S->snap_id  ==1) {
        S->ref_count -=1;
    } else {
        S->ref_count -=2;
    }*/

    return status;
}

void* snap_func(void* arg) 
{
    ubatch_t* ubatch1 = (ubatch_t*) arg;
    status_t ret;
    do {
        ret = create_adjacency_snapshot(ubatch1);
        if (eNoWork == ret) usleep(100);
        else if (eEndBatch == ret) break;
    } while(true);
    return 0;
}

int main(int argc, char** argv)
{
    commandLine P(argc, argv, "./test_graph [-f file -m (mmap) <testid>]");
    string idir = P.getOptionValue("-update-dir", "./data/");
    BATCH_SIZE = P.getOptionLongValue("-batch-size", 65536);
    BATCH_MASK = BATCH_SIZE - 1;
    THD_COUNT = P.getOptionLongValue("-thread-count", 9);
    set_num_workers(THD_COUNT);
    _source = P.getOptionLongValue("-source", 1);
    
    //Initialize the system. 
    //Return ubatch pointer here.
    VG = initialize_treeplus_graph(P);
    versioned_graph<treeplus_graph>::version S = VG.acquire_version();
    const auto& GA = S.graph;
    size_t v_count = GA.num_vertices();
    VG.release_version(std::move(S));

    ubatch_t* ubatch = new ubatch_t(sizeof(edge_t),  1);
    ubatch->alloc_edgelog(20); // 1 Million edges
    ubatch->reg_archiving();
    updates = (pair_vertex*)malloc(sizeof(pair_vertex)*2*BATCH_SIZE);
    deletes = (pair_vertex*)malloc(sizeof(pair_vertex)*2*BATCH_SIZE);
    
    //create typekv as it handles the mapping for vertex id (string to vid_t)
    typekv = new typekv_t;
    typekv->manual_setup(v_count, true, "gtype");
    
    //If system support adjacency store snapshot, create thread for index creation
    INIT_LIST_HEAD(&snapshot_list);
    pthread_t thread;
    if (0 != pthread_create(&thread, 0, snap_func, ubatch)) {
        assert(0);
    }
    
    // Run analytics in separte thread. If adjacency store is non-snapshot, do indexing and analytics in seq.
    index_t slide_sz = BATCH_SIZE;
    gview_t* sstreamh = reg_sstream_view(ubatch, v_count, kickstarter_bfs<dst_id_t>, C_THREAD, slide_sz);
    
    //perform micro batching here using ubatch pointer
    int64_t flags = 0; 
    if (_source == 1) {
        flags = SOURCE_BINARY;
    }
    index_t total_edges = add_edges_from_dir<dst_id_t>(idir, ubatch, flags);
    ubatch->set_total_edges(total_edges);
    
    //Wait for threads to complete
    void* ret;
    pthread_join(sstreamh->thread, &ret);
    pthread_join(thread, &ret);

    return 0;
}
