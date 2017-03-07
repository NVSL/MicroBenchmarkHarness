#include"MicroBenchmarkHarness.hpp"
#include <string>
#include <stdlib.h>
#include <malloc.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>

namespace patch
{
    template < typename T > std::string to_string( const T& n )
    {
        std::ostringstream stm ;
        stm << n ;
        return stm.str() ;
    }
}


std::string filepath = "/mnt/ramdisk/file";
uint64_t  bytesPer1GB = 1024*1024*1024; 
uint64_t  fileLength = 2 * bytesPer1GB; // 1GB by default
uint64_t  blockSize  = 4 * 1024; // 4 KB by default

// Parse our custom options on the command line.
// d - directory/file path
// f - file size
// b - block size
void ParseOptions(int & argc, char  *argv[])
{
     int c;
     /* process arguments */
     while ((c = getopt(argc, argv, "d:f:b:")) != -1) {
          switch (c) {
	  case 'b':
	       blockSize = atoi(optarg);
	       break;
	  case 'f':
	       fileLength = atoi(optarg);
	       break;
          case 'd':
               filepath = optarg;
               break;
          default:
               fprintf(stderr, "Illegal argument \"%c\"\n", c);
               exit(EXIT_FAILURE);
          }
     }
}

// Little struct with the state our benchmark needs in each thread.
struct ThreadArgs {
     uint64_t max_index;
     uint64_t seed;
     int id;
     int fd;
     uint64_t writeSize;
     uint64_t fileSize;
     char *buf;
};

// Barriers to coordinate execution across threads.  The main goal is to make
// sure that some threads don't start running before all of them have been
// created.
nvsl::Barrier *startBarrier;
nvsl::Barrier *endBarrier;
nvsl::Barrier *runBarrier;

void fill_buffer(ThreadArgs * args) {
     char rbyte = (char)RandLFSR(&args->seed) % args->max_index;
     char *buf  = args->buf;
     for (unsigned int i = 0 ; i < args->writeSize ; i++)
	buf[i] = rbyte;
     return;
}

void write_forward(ThreadArgs * args) {
     uint64_t fileSize, writeSize;

     fileSize = args->fileSize;
     writeSize = args->writeSize;

     char *buf = args->buf;

     while (fileSize > 0) {
	if (writeSize > fileSize)
            writeSize = fileSize;

	if (write(args->fd, buf, writeSize) <= 0)
	    break;

        fileSize -= writeSize;
    }

}

// The function each threa runs.  The argument gets passed from StartThread()
// below.  This is exactly what RunOps() does.
void * go(void *arg) {

     // Recover this threads arguments.
     ThreadArgs * args = reinterpret_cast<ThreadArgs*>(arg);

     // Grab the number of ops to perform per thread.
     unsigned int threadOps = nvsl::MicroBenchmarkHarness::GetOperationCountPerThread();

     void (*fptr)(ThreadArgs *);
     fptr = &write_forward;


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
	       fptr(args);
	       c++;
	  }
	  // Tell the system how many operations we completed.
	  nvsl::MicroBenchmarkHarness::CompletedOperations(c);
     } else { // running for a fixed number of ops.

	  // Run the number of ops we should run.
	  for(unsigned int i = 0; i < threadOps; i++) {
	       fptr(args);
	  }
	  // Tell the harness.
	  nvsl::MicroBenchmarkHarness::CompletedOperations(threadOps);

     }

     endBarrier->Join();
     return NULL;
}

int main (int argc, char *argv[]) {


     nvsl::MicroBenchmarkHarness::Init("write", argc, argv);

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

     std::vector<int> fileDesc;

     // Each thread will work on it's own file.
     // Open the file here if we are not interested in measuruing the open
     // operation
     for(unsigned int i= 0; i< thread_count; i++) {
	ThreadArgs * t = new ThreadArgs;
	std::string fileName = filepath + patch::to_string(i+1);
	int fd = open(fileName.c_str(), O_CREAT | O_WRONLY, 0600);
	char *buf = (char *)valloc(blockSize);
	t->max_index = 255;
	t->seed = i;
	t->id = i;
	t->fd  = fd;
	t->writeSize = blockSize;
	t->fileSize = fileLength;
	t->buf = buf;
	fill_buffer(t);
	argsList.push_back(t);
	fileDesc.push_back(fd);
     }

     for(unsigned int i= 0; i< thread_count; i++) {
          nvsl::MicroBenchmarkHarness::StartThread(go,reinterpret_cast<void*>(argsList[i]));
     }

     // wait for all threads to complete.
     nvsl::MicroBenchmarkHarness::WaitForThreads();
     nvsl::MicroBenchmarkHarness::StopTiming();
     nvsl::MicroBenchmarkHarness::PrintResults();

     for (unsigned int i = 0; i < fileDesc.size(); i++)
	close(fileDesc[i]);

     for (unsigned int i = 0; i < argsList.size(); i++)
	free(argsList[i]->buf);

     return 0;
}
