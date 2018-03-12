#include "MicroBenchmarkHarness.hpp"
#include <string>
#include <libpmem.h>
#include <vector>
#include <assert.h>

#define MIN_HEAP_SIZE       64 // MB
#define CACHE_LINE_WIDTH    64 // Bytes

using namespace std;
using namespace nvsl;

// Global barriers
nvsl::Barrier *startBarrier;
nvsl::Barrier *endBarrier;
nvsl::Barrier *runBarrier;

// Global variables
size_t accessSize = CACHE_LINE_WIDTH;
enum AccessMode {
    RandomAccess = 0,
    SequentialAccess = 1
} accessMode = SequentialAccess;

void ParseOptions(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, "m:g:")) != -1) {
        switch (c) {
            case 'm':
                if (strcmp("rnd", optarg) == 0) {
                    accessMode = RandomAccess;
                }
                else if (strcmp("seq", optarg) == 0) {
                    accessMode = SequentialAccess;
                }
                else {
                    fprintf(stderr, "Access mode not supported: '%s'\n",
                            optarg);
                }
                break;
            case 'g':
                accessSize = atoi(optarg);
                assert(accessSize % CACHE_LINE_WIDTH == 0);
                break;
            default:
                fprintf(stderr, "Unexpected argument: %c\n", c);
                exit(EXIT_FAILURE);
        }
    }
}

class ThreadArgs {
public:
    unsigned int threadID;
    void *viewPtr;
    size_t viewLen;
    uint64_t seed;
    uint64_t *buffer;
    uint64_t lastReadBlock; // used for sequential accesses
    size_t totalBlocks; // view length / access size
    size_t quadWordsPerBlock; // block size / sizeof(uint64_t)

    void Print() {
        std::cout << "Thread ID = " << threadID <<
            ", View length = " << viewLen <<
            ", Seed = " << seed <<
            ", Last read block = " << lastReadBlock <<
            ", Total blocks = " << totalBlocks <<
            ", Quad words per block = " << quadWordsPerBlock << endl;
    }
};

void random_read(ThreadArgs *args) {
    uint64_t block = (uint64_t)RandLFSR(&args->seed) % args->totalBlocks;
    uint64_t *ptr = (uint64_t *)((char *)args->viewPtr + block * accessSize);
    for (uint64_t i = 0; i < args->quadWordsPerBlock; i++) {
        args->buffer[i] = ptr[i];
    }
}

void sequential_read(ThreadArgs *args) {
    uint64_t block = args->lastReadBlock;
    args->lastReadBlock = (args->lastReadBlock + accessSize) % args->totalBlocks;
    uint64_t *ptr = (uint64_t *)((char *)args->viewPtr + block * accessSize);
    for (uint64_t i = 0; i < args->quadWordsPerBlock; i++) {
        args->buffer[i] = ptr[i];
    }
}

void *go(void *arg) {

    ThreadArgs *args = reinterpret_cast<ThreadArgs *>(arg);

    // Prepare thread environment
    uint64_t opCount = MicroBenchmarkHarness::GetOperationCountPerThread();
    void (*fptr)(ThreadArgs *) = &sequential_read;
    if (accessMode == RandomAccess) fptr = &random_read;

    // Wait for other threads
    startBarrier->Join();

    // If you are the first thread, start timing
    if (args->threadID == 0) {
        MicroBenchmarkHarness::StartTiming();
    }

    runBarrier->Join();

    // Run benchmark
    uint64_t c = 0;
    if (opCount == 0) { // Fixed time frame
        while (!MicroBenchmarkHarness::isDone()) {
            fptr(args);
            c++;
        }
    }
    else { // Fixed operations
        for (uint64_t i = 0; i < opCount; i++) {
            fptr(args);
        }
        c = opCount;
    }

    MicroBenchmarkHarness::CompletedOperations(c);
    endBarrier->Join();

    return NULL;
}

int main(int argc, char **argv) {

    MicroBenchmarkHarness::Init("DAX/LD", argc, argv);
    MicroBenchmarkHarness::SuspendTiming();

    // Configure benchmark
    ParseOptions(argc, argv);
    assert(MicroBenchmarkHarness::GetFootPrintMB() >= MIN_HEAP_SIZE);
    size_t nvHeapLen = MicroBenchmarkHarness::GetFootPrintBytes();
    string nvHeapPath = MicroBenchmarkHarness::GetFileName();
    assert(nvHeapPath.length() > 0);

    // Create persistent heap
    int isPMEM;
    size_t nvMapLen;
    void *nvHeapPtr = pmem_map_file(nvHeapPath.c_str(), nvHeapLen,
            PMEM_FILE_CREATE | PMEM_FILE_EXCL, 0666, &nvMapLen, &isPMEM);
    assert(nvHeapPtr != NULL);
    assert(nvMapLen == nvHeapLen);
    //assert(isPMEM != 0); TODO

    // Prepare environment
    unsigned int threadCount = MicroBenchmarkHarness::GetThreadCount();
    startBarrier = new nvsl::Barrier(threadCount);
    endBarrier = new nvsl::Barrier(threadCount);
    runBarrier = new nvsl::Barrier(threadCount);

    // Prepare configurations
    vector<ThreadArgs *> threadArgs;
    for (unsigned int i = 0; i < threadCount; i++) {
        ThreadArgs *t = new ThreadArgs;
        t->threadID = i;
        t->viewPtr = nvHeapPtr;
        t->viewLen = nvMapLen;
        t->seed = i;
        t->buffer = (uint64_t *)malloc(accessSize);
        t->lastReadBlock = 0;
        t->totalBlocks = nvMapLen / accessSize;
        t->quadWordsPerBlock = accessSize / sizeof(uint64_t);
        threadArgs.push_back(t);
        //t->Print();
    }

    // Create benchmark threads
    for (unsigned int i = 0; i < threadCount; i++) {
        MicroBenchmarkHarness::StartThread(go,
                reinterpret_cast<void *>(threadArgs[i]));
    }

    // Wait for threads to terminate
    MicroBenchmarkHarness::WaitForThreads();
    MicroBenchmarkHarness::StopTiming();
    MicroBenchmarkHarness::PrintResults();

    // Clean-up
    pmem_unmap(nvHeapPtr, nvMapLen);
    for (unsigned int i = 0; i < threadCount; i++) {
        ThreadArgs *t = threadArgs.back();
        threadArgs.pop_back();
        free(t->buffer);
        delete t;
    }

    return 0;
}
