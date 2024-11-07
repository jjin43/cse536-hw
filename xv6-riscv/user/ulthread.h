#ifndef __UTHREAD_H__
#define __UTHREAD_H__

#include <stdbool.h>

#define MAXULTHREADS 100

enum ulthread_state {
  FREE,
  RUNNABLE,
  YIELD,
};

enum ulthread_scheduling_algorithm {
  ROUNDROBIN,   
  PRIORITY,     
  FCFS,         // first-come-first serve
};

// Thread context
struct ulthread_context {
  uint64 ra;
  uint64 sp;

  // argument registers to pass
  uint64 a0;
  uint64 a1;
  uint64 a2;
  uint64 a3;
  uint64 a4;
  uint64 a5;

  // save caller registers
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Thread structure
struct ulthread {
  int tid;
  enum ulthread_state state;
  struct ulthread_context context;
  int priority;
  uint64 access_time;
} empty_thread = {-1, FREE, {0}, -1, 0};

#endif