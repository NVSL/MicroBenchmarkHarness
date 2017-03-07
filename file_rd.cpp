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
bool randomRead = false;
uint64_t  bytesPer1GB = 1024*1024*1024; 
uint64_t  fileLength = 2 * bytesPer1GB; // 2GB by default
uint64_t  blockSize  = 4 * 1024; // 4 KB by default

// Parse our custom options on the command line.
// d - directory/file path
// r - is random read?
// f - file size
// b - block size
void ParseOptions(int & argc, char  *argv[])
{
     int c;
     /* process arguments */
     while ((c = getopt(argc, argv, "d:rf:b:")) != -1) {
          switch (c) {
          case 'd':
               filepath = optarg;
               break;
          case 'r':
	       randomRead = true;
	       break;
	  case 'f':
	       fileLength = atoi(optarg);
	       break;
	  case 'b':
	       blockSize = atoi(optarg);
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
     uint64_t readSize;
     uint64_t fileSize;
};

// Barriers to coordinate execution across threads.  The main goal is to make
// sure that some threads don't start running before all of them have been
// created.
nvsl::Barrier *startBarrier;
nvsl::Barrier *endBarrier;
nvsl::Barrier *runBarrier;

long crunch(void *buf, long bufSize) {
     long sum = 0;
     register long *start = (long *)buf;

     while(bufSize >= 64) {
	sum +=  start[0] + start[8] + start[16] + start[24] + start[32] +
		start[40] + start[48] + start[56];
	start += 64;
	bufSize -= 64;
     }

     return sum;
}

void read_forward(ThreadArgs * args) {
     uint64_t fileSize, readSize;

     fileSize = args->fileSize;
     readSize = args->readSize;

     char *buf = (char *)valloc(readSize);

     while (fileSize > 0) {
	if (readSize > fileSize)
            readSize = fileSize;

	if (read(args->fd, buf, readSize) <= 0)
	    break;

        (void)crunch(buf, readSize);

        fileSize -= readSize;
    }

    free(buf);
}

void read_backward(ThreadArgs * args) {
     uint64_t fileSize, readSize;

     fileSize = args->fileSize;
     readSize = args->readSize;

     char *buf = (char *)valloc(readSize);

     while (fileSize > 0) {
	if (readSize > fileSize)
            readSize = fileSize;

        if (lseek(args->fd, (off_t)(fileSize - readSize), SEEK_SET) <= 0)
	    break;

	if (read(args->fd, buf, readSize) <= 0)
	    break;

        (void)crunch(buf, readSize);

        fileSize -= readSize;
    }

    free(buf);
}


// The function each threa runs.  The argument gets passed from StartThread()
// below.  This is exactly what RunOps() does.
void * go(void *arg) {

     // Recover this threads arguments.
     ThreadArgs * args = reinterpret_cast<ThreadArgs*>(arg);

     // Grab the number of ops to perform per thread.
     unsigned int threadOps = nvsl::MicroBenchmarkHarness::GetOperationCountPerThread();

     void (*fptr)(ThreadArgs *);
     if (randomRead)
	fptr = &read_backward;
     else
	fptr = &read_forward;


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


     nvsl::MicroBenchmarkHarness::Init("read", argc, argv);

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
	int fd = open(fileName.c_str(), O_RDONLY);
	t->max_index = nvsl::MicroBenchmarkHarness::GetFootPrintBytes()/sizeof(uint64_t)/thread_count;
	t->seed = i;
	t->id = i;
	t->fd  = fd;
	t->readSize = blockSize;
	t->fileSize = fileLength;
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

     return 0;
}
