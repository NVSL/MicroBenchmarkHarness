#include"MicroBenchmarkHarness.hpp"
#include <unistd.h>
#include "FastRand.hpp"


/***

    This is more complex example shows how to manage threads yourself as well
    as setting up and tearing down the microbenchmark.

***/


// Custom options.

bool shared = false;
int modulo = 1;

// Parse our custom options on the command line.
void ParseOptions(int & argc, char  *argv[])
{
     int c;
     /* process arguments */
     while ((c = getopt(argc, argv, "Sm:")) != -1) {
          switch (c) {
          case 'm':
               modulo = atoi(optarg);
               break;
	  case 'S':
	       shared = true;
	       break;
          default:
               fprintf(stderr, "Illegal argument \"%c\"\n", c);
               exit(EXIT_FAILURE);
          }
     }

}

// Little struct with the state our benchmark needs in each thread.
struct ThreadArgs {
     uint64_t * data;
     uint64_t max_index;
     uint64_t seed;
     int id;
};


// Barriers to coordinate execution across threads.  The main goal is to make
// sure that some threads don't start running before all of them have been
// created.
nvsl::Barrier *startBarrier;
nvsl::Barrier *endBarrier;
nvsl::Barrier *runBarrier;


// The core function we want to time.  In this case, we are swapping values in
// a an array.
void op(ThreadArgs * args) {
     uint64_t a = RandLFSR(&args->seed) % args->max_index;
     uint64_t b = RandLFSR(&args->seed) % args->max_index;
     uint64_t t = args->data[a];
     args->data[a] = args->data[b];
     args->data[b] = t;
}


// The function each threa runs.  The argument gets passed from StartThread()
// below.  This is exactly what RunOps() does.
void * go(void *arg) {

     // Recover this threads arguments.
     ThreadArgs * args = reinterpret_cast<ThreadArgs*>(arg);

     // Grab the number of ops to perform per thread.
     unsigned int threadOps = nvsl::MicroBenchmarkHarness::GetOperationCountPerThread();

     // Wait for the threads to be started.
     startBarrier->Join();

     // If you we are the first thread, start timing.
     if (args->id == 0) {
          nvsl::MicroBenchmarkHarness::StartTiming();
     }

     // ready, set, ....
     runBarrier->Join();

     //Go !
     if (threadOps == 0) { // Running for a fixed period of time.
	  
	  uint64_t c = 0;
	  // isDone() checks a couple of termination conditions including 
	  while(!nvsl::MicroBenchmarkHarness::isDone()) {
	       op(args);
	       c++;
	  }
	  // Tell the system how many operations we completed.
	  nvsl::MicroBenchmarkHarness::CompletedOperations(c);
     } else { // running for a fixed number of ops.

	  // Run the number of ops we should run.
	  for(unsigned int i = 0; i < threadOps; i++) {
	       op(args);
	  }
	  // Tell the harness.
	  nvsl::MicroBenchmarkHarness::CompletedOperations(threadOps);

     }

     // The above implementation may not be a deal.  if the latency for op() is
     // variable, we may end up waiting for a slow thread to finish, which will
     // effectively reduce our throughput.
     //
     // An alternative implementation would look like this:
     //
     // while(!nvsl::MicroBenchmarkHarness::isDone()) {
     //     op(args);
     //     nvsl::MicroBenchmarkHarness::CompletedOperations(1);
     // }
     //
     // This will work since isDone() also checks the operation count and will
     // return true once we have executed enough ops.  The problem is that
     // CompletedOperations() has to increment a shared variable and that turns
     // out to be expensive if op() is fast.
     //
     // The right approach depends on your op().
     
     endBarrier->Join();
     return NULL;
}

int main (int argc, char *argv[]) {

     
     nvsl::MicroBenchmarkHarness::Init("gsps", argc, argv); // 'gsps' stands for giga-swaps per second.
     
     nvsl::MicroBenchmarkHarness::SuspendTiming(); // Stop timing because we
						   // are going set up some
						   // stuff we don't want
						   // timed.

     // parse our custom options
     ParseOptions(argc, argv);

     // get thread count and create the barriers.
     uint32_t thread_count = nvsl::MicroBenchmarkHarness::GetThreadCount();
     startBarrier = new nvsl::Barrier(thread_count);
     endBarrier = new nvsl::Barrier(thread_count);
     runBarrier = new nvsl::Barrier(thread_count);

     typedef std::vector<ThreadArgs* > ArgsList;
     
     ArgsList argsList;

     // If shared, then all the threads work on the same array.
     if (shared) {
	  ThreadArgs *p = new ThreadArgs;
	  p->max_index = nvsl::MicroBenchmarkHarness::GetFootPrintBytes()/sizeof(uint64_t);
	  p->data = new uint64_t[p->max_index];
	  p->seed = 1;
	  for(unsigned int i = 0; i < thread_count; i++) {
	       ThreadArgs * t = new ThreadArgs;
	       t->max_index = p->max_index;
	       t->data = p->data;
	       t->seed = i;
	       t->id = i;
	       argsList.push_back(t);
	  }
     } else { // no shared, they get their own array.
	  for(unsigned int i = 0; i < thread_count; i++) {
	       ThreadArgs * t = new ThreadArgs;
	       t->max_index = nvsl::MicroBenchmarkHarness::GetFootPrintBytes()/sizeof(uint64_t)/thread_count;
	       t->data = new uint64_t[t->max_index];
	       t->seed = i;
	       t->id = i;
	       argsList.push_back(t);
	  }
     }
     
     for(unsigned int i= 0; i< thread_count; i++) {
          nvsl::MicroBenchmarkHarness::StartThread(go,reinterpret_cast<void*>(argsList[i]));
     }

     // wait for all threads to complete.
     nvsl::MicroBenchmarkHarness::WaitForThreads();
     nvsl::MicroBenchmarkHarness::StopTiming();
     nvsl::MicroBenchmarkHarness::PrintResults();

     return 0;
}
