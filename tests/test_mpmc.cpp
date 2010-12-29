/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ***************************************************************************
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as 
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  As a special exception, you may use this file as part of a free software
 *  library without restriction.  Specifically, if other files instantiate
 *  templates or use macros or inline functions from this file, or you compile
 *  this file and link it with other files to produce an executable, this
 *  file does not by itself cause the resulting executable to be covered by
 *  the GNU General Public License.  This exception does not however
 *  invalidate any other reasons why the executable file might be covered by
 *  the GNU General Public License.
 *
 ****************************************************************************
 */
/*
 * Simple test for the Multi-Producer/Multi-Consumer MSqueue.
 *
 * Author: Massimo Torquati
 *  December 2010
 * 
 *
 */
#include <iostream>
#include <ff/node.hpp>   // for Barrier
#include <ff/cycle.h>
#include <ff/atomic/atomic.h>

using namespace ff;

#if defined(USE_LFDS)
// you need to install liblfds if you want to 
// test the MSqueue implementation contained
// in that library - www.liblfds.org
extern "C" {
#include <liblfds.h>
};
struct queue_state *msq;
#else
#include <ff/MPMCqueues.hpp>
MSqueue msq;
#endif


int ntasks=0;            // total number of tasks
bool end = false;
atomic_long_t counter;
std::vector<long> results;

static inline bool PUSH() {
    long * p = (long *)(atomic_long_inc_return(&counter));

    if ((long)p > ntasks) return false;

#if defined(USE_LFDS)
    do ; while( !queue_enqueue(msq, p ) );
#else
    do ; while(!(msq.push(p)));
#endif

    return true;
}   

// producer function
void * P(void *) {
    Barrier::instance()->barrier();
    ffTime(START_TIME);

    do; while(PUSH());

    pthread_exit(NULL);
}
    
// consumer function
void * C(void *) {

    union {long a; void *b;} task;
    
    Barrier::instance()->barrier();

    while(!end) {
        
#if defined(USE_LFDS)
        if (queue_dequeue(msq, &task.b))
#else
        if (msq.pop(&task.b)) 
#endif
        {
            if (task.b == (void*)FF_EOS) end=true;
            else {
                if (task.a > ntasks) {
                    std::cerr << "received " << task.a << " ABORT\n";
                    abort();
                }
                results[task.a-1] = task.a;
            }
        } 
    }
    ffTime(STOP_TIME);
    
    pthread_exit(NULL);
}



int main(int argc, char * argv[]) {
    if (argc!=4) {
        std::cerr << "use: " << argv[0] << " ntasks #P #C\n";
        return -1;
    }
    ntasks= atoi(argv[1]);
    int numP  = atoi(argv[2]);
    int numC  = atoi(argv[3]);

    results.resize(ntasks,-1);


#if defined(USE_LFDS)
    queue_new( &msq, 1000000 );
#else
    msq.init();
#endif

    atomic_long_set(&counter,0);

    pthread_t P_handle[numP], C_handle[numC];

    Barrier::instance()->barrier(numP+numC);

    ffTime(START_TIME);

    for(int i=0;i<numC;++i)        
        if (pthread_create(&C_handle[i], NULL,C,NULL) != 0) {
            abort();
        }

    for(int i=0;i<numP;++i) 
        if (pthread_create(&P_handle[i], NULL,P,NULL) != 0) {
            abort();
        }
    
    // wait all producers
    for(int i=0;i<numP;++i) 
        pthread_join(P_handle[i],NULL);

    // send EOS to stop the consumers
    for(int i=0;i<numC;++i) {
#if defined(USE_LFDS)
        do ; while(! queue_enqueue(msq, (void*)FF_EOS));
#else
        do ; while(! msq.push((void*)FF_EOS)); 
#endif
    }
    // wait all consumers
    for(int i=0;i<numC;++i) 
        pthread_join(C_handle[i],NULL);
    

    std::cout << "Checking result...\n";
    // check result
    bool wrong = false;
    for(int i=0;i<ntasks;++i)
        if (results[i] != i+1) {
            std::cerr << "WRONG result " << results[i] << " should be " << i+i << "\n";
            wrong = true;
        }
    if (!wrong)  std::cout << "Ok. Done!\n";
    return 0;
}