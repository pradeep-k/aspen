#pragma once
#include <stdlib.h>
#include <pthread.h>

using namespace std;

#include "sstream_view.h"
/*
template<class T>
void* sstream_func(void* arg) 
{
    gview_t* sstreamh = (gview_t*)arg;
    sstreamh->sstream_func(sstreamh);
    return 0;
}*/


sstream_t* reg_sstream_view(ubatch_t* ugraph, vid_t v_count, typename callback<dst_id_t>::sfunc func,
                               index_t flag,  index_t slide_sz = 0, void* algo_meta = 0)
{
    sstream_t* sstreamh = new sstream_t;
    
    sstreamh->init_view(ugraph, v_count, flag, slide_sz);
    //sstreamh->sstream_func = func;
    sstreamh->algo_meta = algo_meta;
    
    if (IS_THREAD(flag)) {
        if (0 != pthread_create(&sstreamh->thread, 0, func, sstreamh)) {
            assert(0);
        }
        cout << "created sstream thread" << endl;
    }
    
    return sstreamh;
}
    
void unreg_view(gview_t* viewh)
{
    delete viewh;
}

