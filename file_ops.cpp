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

enum file_operations {
    createOp, /* 0 */
    create_writeOp, /* 1 */
    renameOp /* 2 */
};

std::string filepath = "/mnt/ramdisk/";
int pageSize = 4096;
int numFiles = 1;
std::string newFilePath = "/mnt/ramdisk/";
file_operations fileOp  = createOp;

// Parse our custom options on the command line.
// d - directory/file path
// f - file size
// b - block size
void ParseOptions(int & argc, char  *argv[])
{
     int c;
     /* process arguments */
     while ((c = getopt(argc, argv, "o:d:n:f:")) != -1) {
          switch (c) {
	  case 'o':
		fileOp = static_cast<file_operations>(atoi(optarg));
		break;
	  case 'd':
	       filepath = optarg;
	       break;
	  case 'n':
	  	numFiles = atoi(optarg);
		break;
	  case 'f':
		newFilePath = optarg;
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
     int start_index;
     int end_index;
     std::string fileName;
     std::string newFileName;
     int id;
     int blockSize;
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
     for (int i = 0 ; i < args->blockSize ; i++)
	buf[i] = rbyte;
     return;
}

void fcreate(ThreadArgs *args) {
	for (int i = args->start_index; i <= args->end_index; i++) {
	    std::string file = args->fileName + patch::to_string(i);
	    int fd = open(file.c_str(), O_CREAT | O_RDWR, 0600);
	    close(fd);
	} 
}

void fcreate_write(ThreadArgs *args) {
	for (int i = args->start_index; i <= args->end_index; i++) {
	    std::string file = args->fileName + patch::to_string(i);
	    int fd = open(file.c_str(), O_CREAT | O_RDWR, 0600);
	    if (write(fd, args->buf, args->blockSize) <= 0)
		break;
	    close(fd);
	} 
}

void frename(ThreadArgs *args) {
	for (int i = args->start_index; i <= args->end_index; i++) {
	   std::string oldPath = args->fileName + patch::to_string(i);
	   std::string newPath = args->newFileName + patch::to_string(i);
	   if (rename(oldPath.c_str(), newPath.c_str()) < 0)
		break;
	}
}

// The function each thread runs.  The argument gets passed from StartThread()
// below.  This is exactly what RunOps() does.
void * go(void *arg) {

     // Recover this threads arguments.
     ThreadArgs * args = reinterpret_cast<ThreadArgs*>(arg);

     // Grab the number of ops to perform per thread.
     unsigned int threadOps = nvsl::MicroBenchmarkHarness::GetOperationCountPerThread();

     void (*fptr)(ThreadArgs *);
     switch(fileOp) {
	case createOp : fptr = &fcreate;
		        break;
	case create_writeOp : fptr = &fcreate_write;
			      break;
	case renameOp : fptr = &frename;
		        break;

	default : fptr = &fcreate;
     }	

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


     nvsl::MicroBenchmarkHarness::Init("fileOps", argc, argv);

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
     int last_used = 0;

     for(unsigned int i= 0; i< thread_count; i++) {
	ThreadArgs * t = new ThreadArgs;
	char *buf = (char *)valloc(pageSize);
	t->max_index = 255;
	t->seed = i;
	t->id = i;
	t->start_index = (last_used) + 1;
	t->end_index   = last_used + numFiles;
	t->fileName    = filepath + "dir" + patch::to_string(i+1) + "/file";
	t->newFileName = newFilePath + "dir" + patch::to_string(i+1) + "/f";
	t->blockSize = pageSize;
	t->buf = buf;
	fill_buffer(t);
	argsList.push_back(t);
	last_used += numFiles;
     }

     for(unsigned int i= 0; i< thread_count; i++) {
          nvsl::MicroBenchmarkHarness::StartThread(go,reinterpret_cast<void*>(argsList[i]));
     }

     // wait for all threads to complete.
     nvsl::MicroBenchmarkHarness::WaitForThreads();
     nvsl::MicroBenchmarkHarness::StopTiming();
     nvsl::MicroBenchmarkHarness::PrintResults();

     for (unsigned int i = 0; i < argsList.size(); i++)
	free(argsList[i]->buf);

     return 0;
}
