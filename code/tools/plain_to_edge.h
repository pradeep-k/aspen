#pragma once

#include <string>
#include <dirent.h>
#include <assert.h>
#include <string>
#include <unistd.h>

#include "graph_view.h"
#include "typekv.h"
#include "sgraph.h"
#include "util.h"

using namespace std;

template<class T>
void mem_pagerank_simple(gview_t* snaph, int iteration_count);

template <class T>
void run_pr_simple(pgraph_t* pgraph);
template <class T>
void run_pr(pgraph_t* pgraph);
template <class T>
void run_pr_snb(pgraph_t* pgraph);
template <class T>
void run_prd(pgraph_t* pgraph);
template <class T>
void run_wcc(pgraph_t* pgraph);
template <class T>
void run_bfs(pgraph_t* pgraph, sid_t root = 1);
template <class T>
void run_bfs_snb(pgraph_t* pgraph, sid_t root = 1);
template <class T>
void run_1hop(pgraph_t* pgraph);
template <class T>
void run_2hop(pgraph_t* pgraph);


// Credits to :
// http://www.memoryhole.net/kyle/2012/06/a_use_for_volatile_in_multithr.html
inline float qthread_dincr(float *operand, float incr)
{
    //*operand = *operand + incr;
    //return incr;
    
    union {
       float   d;
       uint32_t i;
    } oldval, newval, retval;
    do {
         oldval.d = *(volatile float *)operand;
         newval.d = oldval.d + incr;
         //__asm__ __volatile__ ("lock; cmpxchgq %1, (%2)"
         __asm__ __volatile__ ("lock; cmpxchg %1, (%2)"
                                : "=a" (retval.i)
                                : "r" (newval.i), "r" (operand),
                                 "0" (oldval.i)
                                : "memory");
    } while (retval.i != oldval.i);
    return oldval.d;
}

inline double qthread_doubleincr(double *operand, double incr)
{
    //*operand = *operand + incr;
    //return incr;
    
    union {
       double   d;
       uint64_t i;
    } oldval, newval, retval;
    do {
         oldval.d = *(volatile double *)operand;
         newval.d = oldval.d + incr;
         //__asm__ __volatile__ ("lock; cmpxchgq %1, (%2)"
         __asm__ __volatile__ ("lock; cmpxchg %1, (%2)"
                                : "=a" (retval.i)
                                : "r" (newval.i), "r" (operand),
                                 "0" (oldval.i)
                                : "memory");
    } while (retval.i != oldval.i);
    return oldval.d;
}

enum enumGraph {
    eUdir = 0,
    eDdir = 1,
    eUnidir = 2,
    eRunidir = 3,
    eDoubleEdge = 4,
    eCreateEID = 8,
    eSNBFormat = 16,
    eNoVsnapThread = 32,
    eNoSnapThread = 64,
    eBinarySource = 128,
    eVertNoCreate = 256,
    eNoSnap = 512,
    eNoDurable = 1024,
    eCreateETime = 2048,
};

template <class T>
class pgraph_manager_t {
  public:
    pgraph_t* pgraph;
    ubatch_t* ubatch;
    
  public:
    pgraph_manager_t() {
        pgraph = 0;
        ubatch = 0;
    }

    pgraph_manager_t(string ipath, index_t& flags, int64_t node_number) {
        if (_source == 1) {
            flags |= eBinarySource;
        }
        if (_format == 1) {
            flags |= eDoubleEdge|eCreateEID;
        }
        create_schema(ipath, flags, node_number);
    }

    inline pgraph_t* get_pgraph() {
        return pgraph;
    }
  public:

    //void schema(int dir);
    void create_schema(string ipath, int64_t flags, int64_t v_count);
    index_t add_edges_from_dir(const string& idirname, int64_t flags);
    index_t add_edges_from_file(const string& filename, int64_t flags);

    void prep_graph2(const string& idirname, const string& odirname);
    void prep_graph_edgelog2(const string& idirname, const string& odirname);
    void prep_graph_edgelog(const string& idirname, const string& odirname);
    
    void prep_graph_adj(const string& idirname, const string& odirname);
    void prep_graph_del(const string& idirname, const string& odirname);
    void prep_graph_mix(const string& idirname, const string& odirname);
    void recover_graph_adj(const string& idirname, const string& odirname);
    
    void waitfor_archive_durable();
    
    void split_files(const string& idirname, const string& odirname);
    
};

/*
template <class T>
void pgraph_manager_t<T>::schema(int dir)
{
    const char* longname = "friend";
    const char* shortname = "friend";
    pgraph_t* info = 0; 
    
    switch(dir) {
        case 0:
            info = new ugraph(sizeof(edgeT_t<T>));
            break;
        case 1:
            info = new dgraph(sizeof(edgeT_t<T>));
            break;
        case 2:
            info = new unigraph(sizeof(edgeT_t<T>));
            break;
        case 3:
            info = new runigraph(sizeof(edgeT_t<T>));
            break;
        default:
            assert(0);
    }
    
    T* dst_ptr = 0; 
    info->create_MPI_type(dst_ptr);

    g->add_pgraph(info, longname, shortname);
    info->flag1 = 1;
    info->flag2 = 1;
    set_pgraph(info);
}
*/
template <class T>
void pgraph_manager_t<T>::create_schema(string idirname, int64_t flags, int64_t v_count)
{
    tid_t tid;
    typekv_t* typekv = g->get_typekv();
    bool vert_no_create = IS_VERT_NOCREATE(flags);
    tid = typekv->manual_setup(v_count, !vert_no_create, "gtype");
    
    const char* relation_name = "friend";
    pgraph = g->create_schema(sizeof(edgeT_t<T>), flags, tid, relation_name);
    ubatch = pgraph->get_ubatch();
    T* dst_ptr = 0; 
    pgraph->create_MPI_type(dst_ptr);

    //add data.
    if (false == idirname.empty()) {
        double start = mywtime();
        add_edges_from_dir(idirname, flags);
    
        //Wait for make and durable graph
        waitfor_archive_durable();

        double end = mywtime();
        cout << "Make graph time = " << end - start << endl;
        g->type_store();
    }
}

/*
template <class T>
void pgraph_manager_t<T>::setup_graph(vid_t v_count, egraph_t egraph_type)
{
    //do some setup for plain graphs
    typekv_t* typekv = g->get_typekv();
    typekv->manual_setup(v_count, true, "gtype");
    pgraph->prep_graph_baseline(egraph_type);
}

template <class T>
void pgraph_manager_t<T>::setup_graph_vert_nocreate(vid_t v_count, egraph_t egraph_type)
{
    //do some setup for plain graphs
    typekv_t* typekv = g->get_typekv();
    typekv->manual_setup(v_count, false, "gtype");
    pgraph->prep_graph_baseline(egraph_type);
}*/

struct arg_t {
    string file;
    void* manager;
};

//void* recovery_func(void* arg); 

template <class T>
void* recovery_func(void* a_arg) 
{
    arg_t* arg = (arg_t*) a_arg; 
    string filename = arg->file;
    pgraph_manager_t<T>* manager = (pgraph_manager_t<T>*)arg->manager;

    pgraph_t* ugraph = manager->get_pgraph();
    blog_t*     blog = ugraph->get_ubatch()->blog;
    
    index_t to_read = 0;
    index_t total_read = 0;
    index_t batch_size = (1L << residue);
    cout << "batch_size = " << batch_size << endl;
        
    FILE* file = fopen(filename.c_str(), "rb");
    assert(file != 0);
    index_t size = fsize(filename);
    index_t edge_count = size/sizeof(edgeT_t<T>);
    
    //Lets set the edge log higher
    index_t new_count = upper_power_of_two(edge_count);
    int blog_shift = ilog2(new_count);
    ugraph->alloc_edgelog(blog_shift);
    cout << "edge_count = " << edge_count << endl;
   
    index_t edge_count2 = file_and_insert<T>(filename, ugraph->get_ubatch(), 0);
    assert(edge_count == edge_count2); 
    
    return 0;
}

template <class T>
void pgraph_manager_t<T>::recover_graph_adj(const string& idirname, const string& odirname)
{
    string idir = idirname;
    index_t batch_size = residue;
    cout << "batch_size = " << batch_size << endl;

    arg_t* arg = new arg_t;
    arg->file = idir;
    arg->manager = this;
    pthread_t recovery_thread;
    if (0 != pthread_create(&recovery_thread, 0, &recovery_func<T> , arg)) {
        assert(0);
    }
    
    blog_t*     blog = ubatch->blog;
    index_t marker = 0;
    //index_t snap_marker = 0;
    index_t size = fsize(idirname);
    index_t edge_count = size/sizeof(edgeT_t<T>);
    
    double start = mywtime();
    
    //Make Graph
    while (0 == blog->blog_head) {
        usleep(20);
    }
    while (marker < edge_count) {
        usleep(20);
        while (marker < blog->blog_head) {
            marker = min(blog->blog_head, marker+batch_size);
            pgraph->create_marker(marker);
            pgraph->create_snapshot();
        }
    }
    delete arg; 
    double end = mywtime ();
    cout << "Make graph time = " << end - start << endl;
}

template <class T>
void pgraph_manager_t<T>::split_files(const string& idirname, const string& odirname)
{
    blog_t*     blog = ubatch->blog;
    
    free(blog->blog_beg);
    blog->blog_beg = 0;
    blog->blog_head  += read_idir(idirname, &blog->blog_beg, false);
    pgraph->set_total_edges(blog->blog_head); 
    
    //Upper align this, and create a mask for it
    index_t new_count = upper_power_of_two(blog->blog_head);
    blog->blog_mask = new_count -1;
    
    double start = mywtime();
    
    //Make Graph
    index_t marker = 0;
    index_t batch_size = blog->blog_head/residue;
    cout << "file_counts = " << residue << endl;
    char tmp[32];
    string ifile;
    FILE*  fd = 0;

    for (int i = 1; i < residue; ++i) {
        sprintf(tmp, "%d.dat",i);
        ifile = odirname + "part" + tmp; 
        fd = fopen(ifile.c_str(), "wb");
        fwrite(blog->blog_beg+marker, sizeof(edgeT_t<T>), batch_size, fd); 
        marker += batch_size;
    } 
        sprintf(tmp, "%ld.dat", residue);
        ifile = odirname + "part" + tmp; 
        fd = fopen(ifile.c_str(), "wb");
        fwrite(blog->blog_beg+marker, sizeof(edgeT_t<T>), blog->blog_head - marker, fd); 

    double end = mywtime ();
    cout << "Split time = " << end - start << endl;
}
template <class T>
void* archive(void* arg)
{
    pgraph_t* pgraph = (pgraph_t*)arg;
    
    //Make Graph
    index_t marker = 0;
    index_t batch_size = residue;
    cout << "batch_size = " << batch_size << endl;

    int round = 0;
    double end1, end2;
    double start = mywtime();
    while (marker < pgraph->get_total_edges()) {
        //marker = min(blog->blog_head, marker+batch_size);
        marker = min(pgraph->get_total_edges(), marker+batch_size);
        pgraph->create_marker(marker);
        end1 = mywtime();
        pgraph->create_snapshot();
        end2 = mywtime();
        ++round;
        //cout << round << " Mini-Batch = " << marker << ":" << end2 - end1 << endl;
    }
    double end = mywtime();
    cout << "Make graph time = " << end - start << endl;
    
    return 0;
}

template <class T>
void pgraph_manager_t<T>::prep_graph_adj(const string& idir, const string& odir)
{
    prep_graph_edgelog(idir, odir);
    archive<T>((void*)pgraph);
}

template <class T>
void pgraph_manager_t<T>::prep_graph_mix(const string& idir, const string& odir)
{
    prep_graph_edgelog(idir, odir);
    
    
    //Make Graph
    index_t marker = 0;
    index_t batch_size = residue;
    //marker = blog->blog_head - batch_size;
    cout << "edge counts in basefile = " << batch_size << endl;
    
    index_t unit_size = (1L<<23);
    double start = mywtime();
    while (marker < batch_size ) {
        marker = min(batch_size, marker+ unit_size);
        pgraph->create_marker(marker);
        pgraph->create_snapshot();
    }
    
    double end = mywtime ();
    cout << "Make graph time = " << end - start << endl;
}

template <class T>
void pgraph_manager_t<T>::prep_graph2(const string& idirname, const string& odirname)
{
    char* buf = 0;
    index_t size = read_text_dir(idirname, buf);
    
    //Batch and Make Graph
    index_t edge_count = 0;
    double start = mywtime();
    if (0 == _source) {//text
        edge_count = parsebuf_and_insert<T>(buf, ubatch, size, 0);
    } else {//binary
        edge_count = buf_and_insert<T>(buf, ubatch, size, 0);
    }
    double end1 = mywtime();
    pgraph->set_total_edges(edge_count);
    
    //Wait for make and durable graph
    waitfor_archive_durable();
    double end = mywtime();
    cout << "Batch Update Time (In-Memory) = " << end1 - start << " Edges =" << pgraph->get_total_edges() << endl;
    cout << "Make graph time = " << end - start << endl;
    
    if (false == odirname.empty()) g->type_store();
}

template <class T>
void pgraph_manager_t<T>::prep_graph_edgelog2(const string& idirname, 
        const string& odirname) 
{
    blog_t*     blog = ubatch->blog;
    
    index_t size = fsize_dir(idirname);
    if (size > blog->blog_count) {
        //Upper align this, and create a mask for it
        index_t new_count = upper_power_of_two(size/sizeof(edgeT_t<T>));
        index_t blog_shift = ilog2(new_count);
        ubatch->alloc_edgelog(blog_shift);
    }
       
    char* buf = 0;
    read_text_dir(idirname, buf);

    //Batch Graph
    double start = mywtime();
    double end = mywtime();
    index_t edge_count = 0;
    if (0 == _source) {//text
        start = mywtime();
        edge_count = parsebuf_and_insert<T>(buf, ubatch, size, 0);
        end = mywtime();
    } else {//binary
        start = mywtime();
        edge_count = buf_and_insert<T>(buf, ubatch, size, 0);
        end = mywtime();
    }
    pgraph->set_total_edges(edge_count);
    cout << " Logging Time = " << end - start << " Edge = " << edge_count <<  endl;

    if (false == odirname.empty()) g->type_store();
}

template <class T>
void pgraph_manager_t<T>::prep_graph_edgelog(const string& idirname, 
        const string& odirname)
{
    blog_t*     blog = ubatch->blog;
    
    index_t size = fsize_dir(idirname);
    if (size > blog->blog_count) {
        //Upper align this, and create a mask for it
        index_t new_count = upper_power_of_two(size/sizeof(edgeT_t<T>));
        index_t blog_shift = ilog2(new_count);
        ubatch->alloc_edgelog(blog_shift);
    }
    
    int64_t flags = 0;
    if(_source ==1) {
        flags |= eBinarySource;
    }
    index_t edge_count = add_edges_from_dir(idirname, flags);    
    pgraph->set_total_edges(edge_count);
    
    if (false == odirname.empty()) g->type_store();
}

template <class T>
index_t pgraph_manager_t<T>::add_edges_from_dir(const string& idirname, int64_t flags)
{
    //Batch Graph
    double start = mywtime();
    index_t edge_count = 0;
    if (0 == IS_SOURCE_BINARY(flags)) {//text
        edge_count = read_idir_text<T>(idirname, ubatch, parsefile_and_insert<T>, flags);
    } else {//binary
        edge_count = read_idir_text<T>(idirname, ubatch, file_and_insert<T>, flags);
    }

    double end = mywtime();
    cout << "Batch Update Time (File) = " << end - start  
         << " Edge_count = " << edge_count   
         << " THD_COUNT = " << THD_COUNT << endl;
    return edge_count;
}

template <class T>
index_t pgraph_manager_t<T>::add_edges_from_file(const string& filename, int64_t flags)
{
    //Batch Graph
    double start = mywtime();
    index_t edge_count = 0;
    if (0 == IS_SOURCE_BINARY(flags)) {//text
        edge_count = parsefile_and_insert<T>(filename, ubatch, flags);
    } else {//binary
        edge_count = file_and_insert<T>(filename, ubatch, flags);
    }

    double end = mywtime();
    cout << "Batch Update Time = " << end - start
         << " Vertex_count = " << g->get_type_vcount(0)
         << " Edge_count = " << edge_count   
         << " THD_COUNT = " << THD_COUNT << endl;
    return edge_count;
}

/*
template <class T>
void pgraph_manager_t<T>::setup_threads(const string& odirname, int64_t flags)
{
    if (false == odirname.empty()) {
        pgraph->file_open(odirname, true);
        pgraph->create_wthread();
    }

    bool call_create = !IS_NO_VSNAP_THREAD(flags);
    bool call_screate = !IS_NO_SNAP_THREAD(flags);

    if(call_create) {
        pgraph->create_snapthread(call_screate);//create vsnaps,  snapshot on screate
    }
}
template <class T>
void pgraph_manager_t<T>::prep_graph_ilog(const string& idirname, 
        const string& odirname, int64_t flags)
{
    setup_threads(odirname, flags);
    if(_source ==1) {
        flags |= eBinarySource;
    }
    index_t edge_count = add_edges_from_dir(idirname, flags);    
    pgraph->set_total_edges(edge_count);

    if (false == odirname.empty()) g->type_store();
}*/

/*
template <class T>
void pgraph_manager_t<T>::prep_graph(const string& idirname, const string& odirname, int64_t flags)
{
    CorePin(0);
    double start = mywtime();
   
    setup_threads(odirname, flags);
    if(_source ==1) {
        flags |= eBinarySource;
    }
    index_t edge_count = add_edges_from_dir(idirname, flags);
    pgraph->set_total_edges(edge_count);
    
    //Wait for make and durable graph
    waitfor_archive_durable();

    double end = mywtime();
    cout << "Make graph time = " << end - start << endl;
    if (false == odirname.empty()) g->type_store();
}*/

template <class T>
void pgraph_manager_t<T>::waitfor_archive_durable() 
{
    pgraph->set_total_edges();
    pgraph->waitfor_archive();
    pgraph->waitfor_durable();
}
