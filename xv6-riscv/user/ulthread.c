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
struct ulthread all_threads[MAXULTHREADS];
int num_threads = 0;
int uid = 0;

struct ulthread default_thread = {-1, FREE, {0}, -1, 0};

// Scheduling algorithms
int Roundrobin(enum ulthread_state target_state) {
    
    int target = -1;

    for(int i=curr_thread->tid+1; i < num_threads; i++) {
        if(all_threads[i].state == target_state) {
            target = i;
            break;
        }
    }

    if(target == -1) {
        for(int i=1; i < curr_thread->tid; i++) {
            if(all_threads[i].state == target_state) {
                target = i;
                break;
            }
        }
    }

    printf("[DEBUG] target: %d\n", target);

    return target;
}

int Priority(enum ulthread_state target_state) {

    int target = -1;

    for(int i=1; i < num_threads; i++) {

        if(all_threads[i].state == target_state) {

            if(target == -1) {
                target = i;

            } else if(all_threads[i].priority > all_threads[target].priority) {
                target = i;

            }
            else if(all_threads[i].priority == all_threads[target].priority) {

                if(all_threads[i].access_time < all_threads[target].access_time) {
                    target = i;
                }

            }
        }
    }

    return target;
}

int Fcfs(enum ulthread_state target_state) {

    int target = -1;

    for(int i=1; i < num_threads; i++) {
        if(all_threads[i].state == target_state) {

            if(target == -1) {
                target = i;

            } else {

                if(all_threads[i].access_time < all_threads[target].access_time) {
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
        all_threads[i] = default_thread;
    }


    // Initialize the user scheduler thread
    all_threads[0].tid = uid++;
    all_threads[0].state = RUNNABLE;
    num_threads++;
    curr_thread = &all_threads[0];
    
    // printf("[DEBUG] curr_thread->tid: %d, state:%d\n", curr_thread->tid, curr_thread->state);
}

/* Thread creation */
bool ulthread_create(uint64 start, uint64 stack, uint64 args[], int priority) {

    int i;
    for (i = 1; i < MAXULTHREADS; i++) {

        // Find free thread and load context
        if (all_threads[i].state == FREE) {
            all_threads[i].tid = uid++;
            all_threads[i].state = RUNNABLE;
            all_threads[i].priority = priority;
            all_threads[i].access_time = ctime();

            all_threads[i].context.ra = start;
            all_threads[i].context.sp = stack;
            all_threads[i].context.a0 = args[0];
            all_threads[i].context.a1 = args[1];
            all_threads[i].context.a2 = args[2];
            all_threads[i].context.a3 = args[3];
            all_threads[i].context.a4 = args[4];
            all_threads[i].context.a5 = args[5];

            num_threads++;
            break;
        }
    }
    
    /* Please add thread-id instead of '0' here. */
    printf("[*] ultcreate(tid: %d, ra: %p, sp: %p)\n", all_threads[i].tid, start, stack);
    return true;
}

/* Thread scheduler */
void ulthread_schedule(void) {

    while (num_threads > 1)
    {
        int next_index = -1;
        enum ulthread_state target_state = RUNNABLE;

        printf("[DEBUG] num_threads: %d\n", num_threads);
        printf("[DEBUG] curr_algo: %d\n", curr_algo);

        Schedule_again:

        switch (curr_algo)
        {
            case ROUNDROBIN:
                /* code */
                next_index = Roundrobin(target_state);
                break;

            case PRIORITY:
                next_index = Priority(target_state);
                break;

            case FCFS:
                next_index = Fcfs(target_state);
                break;
            
            default:
                printf("[DEBUG] Unexpected scheduling algorithm #.\n");
                break;
        }


        if(next_index == -1) {
            printf("[DEBUG] No thread to schedule.\n");
            if(target_state == RUNNABLE)
                target_state = YIELD;

            goto Schedule_again;
            
        }

        if(target_state == YIELD){
            all_threads[next_index].state = RUNNABLE;
        }

        /* Add this statement to denote which thread-id is being scheduled next */
        printf("[*] ultschedule (next tid: %d)\n", all_threads[next_index].tid);
        // printf("[DEBUG] next_index: %d\n", next_index);
        
        // Switch between thread contexts
        curr_thread = &all_threads[next_index];
        // printf("[schedule DEBUG] curr_thread->tid: %d, state:%d\n", curr_thread->tid, curr_thread->state);
        
        ulthread_context_switch(&(all_threads[0].context), &(all_threads[next_index].context));
        // printf("[DEBUG] Returned to Scheduler\n");
        // printf("curr_thread->tid: %d, state:%d\n", curr_thread->tid, curr_thread->state);
    }
}

/* Yield CPU time to some other thread. */
void ulthread_yield(void) {

    if (curr_thread->tid == 0) {
        return;
    }
    else{
        curr_thread->state = YIELD;
    }

    /* Please add thread-id instead of '0' here. */
    printf("[*] ultyield(tid: %d)\n", curr_thread->tid);

    ulthread_context_switch(&(curr_thread->context), &(all_threads[0].context));
}

/* Destroy thread */
void ulthread_destroy(void) {
    //printf("[destroy DEBUG] curr_thread->tid: %d, state:%d\n", curr_thread->tid, curr_thread->state);

    if(curr_thread->tid == 0) {
        printf("[DEBUG] Cannot destroy main thread.\n");
        return;
    }

    printf("[*] ultdestroy(tid: %d)\n", curr_thread->tid);

    curr_thread->state = FREE;
    struct ulthread_context temp = curr_thread->context;
    curr_thread = &all_threads[0];
    num_threads--;
    printf("[DEBUG] num_threads: %d\n", num_threads);
    ulthread_context_switch(&temp, &(all_threads[0].context));
}
