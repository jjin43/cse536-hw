/* CSE 536: User-Level Threading Library */
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "user/ulthread.h"

/* Standard definitions */
#include <stdbool.h>
#include <stddef.h> 

enum ulthread_scheduling_algorithm curr_algo;
struct ulthread* curr_thread = 0;
struct ulthread all_thread[MAXULTHREADS];
int num_threads = 0;
int uid = 0;

struct ulthread default_thread = {-1, FREE, {0}, -1, 0};

// Scheduling algorithms
int Roundrobin(void) {
    
    int target = -1;

    for(int i=curr_thread+1; i < num_threads; i++) {
        if(all_thread[i].state == RUNNABLE) {
            target = i;
            break;
        }
    }

    if(target == -1) {
        for(int i=1; i < curr_thread; i++) {
            if(all_thread[i].state == RUNNABLE) {
                target = i;
                break;
            }
        }
    }

    return target;
}

int Priority(void) {

    int target = -1;

    for(int i=1; i < num_threads; i++) {

        if(all_thread[i].state == RUNNABLE) {

            if(target == 0) {
                target = i;

            } else if(all_thread[i].priority < all_thread[target].priority) {
                target = i;

            }
            else if(all_thread[i].priority == all_thread[target].priority) {

                if(all_thread[i].access_time < all_thread[target].access_time) {
                    target = i;
                }

            }
        }
    }

    return target;
}

int Fcfs(void) {

    int target = -1;

    for(int i=1; i < num_threads; i++) {
        if(all_thread[i].state == RUNNABLE) {

            if(target == -1) {
                target = i;

            } else {

                if(all_thread[i].access_time < all_thread[target].access_time) {
                    target = i;
                }

            }

        }
    }

    return target;
}

/* Get thread ID */
int get_current_tid(void) {
    return curr_thread->tid;
}

/* Thread initialization */
void ulthread_init(int schedalgo) {

    curr_algo = schedalgo;    

    // Initialize memory
    for (int i = 0; i < MAXULTHREADS; i++) {
        all_thread[i] = default_thread;
    }


    // Initialize the user scheduler thread
    all_thread[0].tid = uid++;
    all_thread[0].state = RUNNABLE;
    num_threads++;
    curr_thread = &all_thread[0];
    
    printf("[DEBUG] curr_thread->tid: %d, state:%d\n", curr_thread->tid, curr_thread->state);
}

/* Thread creation */
bool ulthread_create(uint64 start, uint64 stack, uint64 args[], int priority) {

    int i = 0;
    for (i = 1; i < MAXULTHREADS; i++) {

        // Find free thread and load mem
        if (all_thread[i].state == FREE) {
            all_thread[i].tid = uid++;
            all_thread[i].state = RUNNABLE;
            all_thread[i].priority = priority;
            all_thread[i].access_time = ctime();

            all_thread[i].context.ra = start;
            all_thread[i].context.sp = stack;
            all_thread[i].context.a0 = args[0];
            all_thread[i].context.a1 = args[1];
            all_thread[i].context.a2 = args[2];
            all_thread[i].context.a3 = args[3];
            all_thread[i].context.a4 = args[4];
            all_thread[i].context.a5 = args[5];

            num_threads++;
            break;
        }
    }
    
    /* Please add thread-id instead of '0' here. */
    printf("[*] ultcreate(tid: %d, ra: %p, sp: %p)\n", all_thread[i].tid, start, stack);
    return false;
}

/* Thread scheduler */
void ulthread_schedule(void) {
    int next_index = -1;

    switch (curr_algo)
    {
        case ROUNDROBIN:
            /* code */
            next_index = Roundrobin();
            break;

        case PRIORITY:
            next_index = Priority();
            break;

        case FCFS:
            next_index = Fcfs();
            break;
        
        default:
            printf("[DEBUG] Unexpected scheduling algorithm #.\n");
            break;
    }
    
    /* Add this statement to denote which thread-id is being scheduled next */
    printf("[*] ultschedule (next tid: %d)\n", all_thread[next_index].tid);
    printf("[DEBUG] next_index: %d\n", next_index);
    // Switch between thread contexts
    curr_thread = &all_thread[next_index];
    printf("[schedule DEBUG] curr_thread->tid: %d, state:%d\n", curr_thread->tid, curr_thread->state);
    ulthread_context_switch(&(all_thread[0].context), &(all_thread[next_index].context));

}

/* Yield CPU time to some other thread. */
void ulthread_yield(void) {

    /* Please add thread-id instead of '0' here. */
    printf("[*] ultyield(tid: %d)\n", 0);
}

/* Destroy thread */
void ulthread_destroy(void) {
    printf("[destroy DEBUG] curr_thread->tid: %d, state:%d\n", curr_thread->tid, curr_thread->state);


    if(curr_thread->tid == 0) {
        printf("[DEBUG] Cannot destroy main thread.\n");
        return;
    }

    printf("[*] ultdestroy(tid: %d)\n", curr_thread->tid);

    curr_thread->state = FREE;
    ulthread_context_switch(&(curr_thread->context), &(all_thread[0].context));
    curr_thread = &all_thread[0];
    num_threads--;

}
