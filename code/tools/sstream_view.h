#pragma once

#include <list>
#include "code/graph/api.h"
#include "code/trees/utils.h"
#include "code/lib_extensions/sparse_table_hash.h"
#include "code/pbbslib/random_shuffle.h"

#include "view_interface.h"

using edge_struct = typename treeplus_graph::edge_struct;
extern versioned_graph<treeplus_graph> VG;
//extern list<versioned_graph<treeplus_graph>::version> snapshot_list;
extern list_head snapshot_list; 

class adj_snap_t {
public:
    list_head list;
    versioned_graph<treeplus_graph>::version* S;
    index_t snap_id;
    int ref_count;

    /*adj_snap_t(versioned_graph<treeplus_graph>::version& ver) {
        S = ver;
    }*/
    adj_snap_t* get_next() {
        return (adj_snap_t*)list.get_next();
    }
    adj_snap_t* get_prev() {
        return (adj_snap_t*)list.get_prev();
    }
};

class sstream_t : public gview_t {
 public: 
    //pbbs::sequence<edge_struct> vtxs;
    //list<versioned_graph<treeplus_graph>::version>::iterator S;
    //versioned_graph<treeplus_graph>::version *S;
    //traversable_graph<treeplus_graph>* pgraph;
    adj_snap_t* S;
    treeplus_graph* pgraph;

    
 public:
    virtual degree_t get_nebrs_out(vid_t vid, nebr_reader_t& header) {
        /*degree_t degree = get_degree_out(vid);
        header.adjust_size(degree, header.T_size);
        return pgraph->edge_map_one(vid, vtxs[vid], (uintV*)header.ptr);
        */
        auto mv = pgraph->find_vertex(vid);
        degree_t degree = mv.value.size();
        header.adjust_size(degree, header.T_size);
        return pgraph->edge_map_one(vid, mv.value, (uintV*)header.ptr);
    }
    virtual degree_t get_nebrs_in (vid_t vid, nebr_reader_t& header) {
        /*degree_t degree = get_degree_in(vid);
        header.adjust_size(degree, header.T_size);
        return pgraph->edge_map_one(vid, vtxs[vid], (uintV*)header.ptr);
        */
        auto mv = pgraph->find_vertex(vid);
        degree_t degree = mv.value.size();
        header.adjust_size(degree, header.T_size);
        return pgraph->edge_map_one(vid, mv.value, (uintV*)header.ptr);
    }
    virtual degree_t get_degree_out(vid_t vid) {
        return pgraph->find_vertex(vid).value.size();
        //return vtxs[vid].size(); 
    }
    virtual degree_t get_degree_in (vid_t vid) {
        return pgraph->find_vertex(vid).value.size();
        //return vtxs[vid].size(); 
    }
    
    virtual status_t    update_view() {
        status_t ret = update_view_help();
        if (ret == eEndBatch) return ret;

        //Get the correct version from VG
        /*if (S != 0) {
            list_del(&S->list);
            VG.release_version1(S->S);
            delete(S->S);
            free(S);
        }
        S = (adj_snap_t*)snapshot_list.get_prev();//the beginning
        */
        
        if(S == 0) {
            while (snapshot_list.get_prev() == &snapshot_list) {
                sleep(100);
            }
            S = (adj_snap_t*)snapshot_list.get_prev();//the beginning
            S->ref_count += 1;//first snapshot is special.
        } else {
            adj_snap_t* it = S;
            while (S->get_prev() == (adj_snap_t*)&snapshot_list) {
                sleep(100);
            }
            S = S->get_prev();//S++
            S->ref_count +=2;
            it->ref_count -= 2;
            
        }

        assert(S->snap_id == vsnapshot->id);
        pgraph =  &(S->S->graph);
        //pgraph->get_all_vertices(vtxs);

        return eOK;
    }
    virtual void        update_view_done() {assert(0);}
    void    init_view(ubatch_t* a_ubatch, vid_t a_vcount, index_t a_flag, index_t slide_sz1) {
        //set array members to 0;
        gview_t::init_view(a_ubatch, a_vcount, a_flag, slide_sz1);
        
        //vtxs = pbbs::sequence<edge_struct>(v_count);
        
        S = 0;
    }
    
   
    inline sstream_t() {
    } 
    inline virtual ~sstream_t() {
        //We may need to free some internal memory.XXX
    } 
};

