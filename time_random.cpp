#include"MicroBenchmarkHarness.hpp"
#include <unistd.h>
#include "FastRand.hpp"

// functions that perfoms the operation we want to measure.  This function
// needs to be stateless aside from arg and seed, since any static or global
// variables it uses will be shared across all the threads.

void op(int id, void *arg, uint64_t & seed) {
    RandLFSR(&seed);
}

int main(int argc, char *argv[]) {
     nvsl::MicroBenchmarkHarness::Init("test", argc, argv);

     // If you need parse command line args, do it here.

     // This will run op() repeatedly with the number of threads and for the lengt of time specifiied via command line args.
     nvsl::MicroBenchmarkHarness::RunOps(op, NULL);

     // Dump results.
     nvsl::MicroBenchmarkHarness::PrintResults(std::cout);
     
     return 0;
}
