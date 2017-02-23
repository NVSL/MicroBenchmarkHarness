#include"MicroBenchmarkHarness.hpp"
#include <unistd.h>
#include "FastRand.hpp"

bool shared = false;
int modulo = 1;
     
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

struct ThreadArgs {
     uint64_t * data;
     uint64_t max_index;
     uint64_t seed;
     int id;
};

nvsl::Barrier *startBarrier;
nvsl::Barrier *endBarrier;
nvsl::Barrier *runBarrier;


void op(ThreadArgs * args) {
     uint64_t a = RandLFSR(&args->seed) % args->max_index;
     uint64_t b = RandLFSR(&args->seed) % args->max_index;
     uint64_t t = args->data[a];
     args->data[a] = args->data[b];
     args->data[b] = t;
}

void * go(void *go) {
     ThreadArgs * args = reinterpret_cast<ThreadArgs*>(go);
     unsigned int threadOps = nvsl::MicroBenchmarkHarness::GetOperationCountPerThread();

     startBarrier->Join();

     if (args->id == 0) {
          nvsl::MicroBenchmarkHarness::StartTiming();
     }

     runBarrier->Join();

     
     if (threadOps == 0) {
	  uint64_t c = 0;
	  while(!nvsl::MicroBenchmarkHarness::isDone()) {
	       op(args);
	       c++;
	  }
	  nvsl::MicroBenchmarkHarness::CompletedOperations(c);
     } else {
	  for(int i = 0; i < threadOps; i++) {
	       op(args);
	  }
	  nvsl::MicroBenchmarkHarness::CompletedOperations(threadOps);
     }


     endBarrier->Join();
     return NULL;
}

int main (int argc, char *argv[]) {

     nvsl::MicroBenchmarkHarness::Init("bdb", argc, argv);
     nvsl::MicroBenchmarkHarness::SuspendTiming(); // Stop timing because we are going set up some stuff.

     ParseOptions(argc, argv);

     uint32_t thread_count = nvsl::MicroBenchmarkHarness::GetThreadCount();
     startBarrier = new nvsl::Barrier(thread_count);
     endBarrier = new nvsl::Barrier(thread_count);
     runBarrier = new nvsl::Barrier(thread_count);

     typedef std::vector<ThreadArgs* > ArgsList;
     
     ArgsList argsList;

          
     if (shared) {
	  ThreadArgs *p = new ThreadArgs;
	  p->max_index = nvsl::MicroBenchmarkHarness::GetFootPrintBytes()/sizeof(uint64_t);
	  p->data = new uint64_t[p->max_index];
	  p->seed = 1;
	  for(int i = 0; i < thread_count; i++) {
	       ThreadArgs * t = new ThreadArgs;
	       t->max_index = p->max_index;
	       t->data = p->data;
	       t->seed = i;
	       t->id = i;
	       argsList.push_back(t);
	  }
     } else {
	  for(int i = 0; i < thread_count; i++) {
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

     nvsl::MicroBenchmarkHarness::WaitForThreads();
     nvsl::MicroBenchmarkHarness::StopTiming();
     nvsl::MicroBenchmarkHarness::PrintResults();

     return 0;
}
