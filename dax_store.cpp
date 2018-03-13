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
enum StoreMode {
    StoreNoBarrier = 0,
    StoreAndBarrier = 1,
    StoreAndFlush = 2,
    NonTempStoreNoBarrier = 3,
    NonTempStoreAndBarrier = 4
} storeMode = StoreNoBarrier;

void ParseOptions(int argc, char **argv) {
    int c;
    while ((c = getopt(argc, argv, "m:g:s:")) != -1) {
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
            case 's':
                if (strcmp("no-barrier", optarg) == 0) {
                    storeMode = StoreNoBarrier;
                }
                else if (strcmp("barrier", optarg) == 0) {
                    storeMode = StoreAndBarrier;
                }
                else if (strcmp("flush", optarg) == 0) {
                    storeMode = StoreAndFlush;
                }
                else if (strcmp("nstore-no-barrier", optarg) == 0) {
                    storeMode = NonTempStoreNoBarrier;
                }
                else if (strcmp("nstore-barrier", optarg) == 0) {
                    storeMode = NonTempStoreAndBarrier;
                }
                else {
                    fprintf(stderr, "Access mode not supported: '%s'\n",
                            optarg);
                }
                break;
            default:
                fprintf(stderr, "Unexpected argument: %c\n", c);
                exit(EXIT_FAILURE);
        }
    }
}

// Barrier functions
void barrier_empty(void *addr, size_t size) {
    // Empty
}
void barrier_sfence(void *addr, size_t size) {
    asm volatile("sfence" : : : "memory");
}
void barrier_flush(void *addr, size_t size) {
    pmem_persist(addr, size);
}

class ThreadArgs {
public:
    unsigned int threadID;
    void *viewPtr;
    size_t viewLen;
    uint64_t seed;
    uint64_t *buffer;
    void *(*memcpyPtr)(void *, const void *, size_t);
    void (*barrierPtr)(void *, size_t);
    uint64_t nextBlockToWrite;
    size_t totalBlocks; // view length / access size
};

void random_write(ThreadArgs *args) {
    uint64_t block = (uint64_t)RandLFSR(&args->seed) % args->totalBlocks;
    uint64_t *ptr = (uint64_t *)((char *)args->viewPtr + block * accessSize);
    args->memcpyPtr(ptr, args->buffer, accessSize);
    args->barrierPtr(ptr, accessSize);
}

void sequential_write(ThreadArgs *args) {
    uint64_t block = args->nextBlockToWrite;
    args->nextBlockToWrite = (args->nextBlockToWrite + accessSize) % args->totalBlocks;
    uint64_t *ptr = (uint64_t *)((char *)args->viewPtr + block * accessSize);
    args->memcpyPtr(ptr, args->buffer, accessSize);
    args->barrierPtr(ptr, accessSize);
}

void *go(void *arg) {

    ThreadArgs *args = reinterpret_cast<ThreadArgs *>(arg);

    // Prepare thread environment
    uint64_t opCount = MicroBenchmarkHarness::GetOperationCountPerThread();
    void (*fptr)(ThreadArgs *) = &sequential_write;
    if (accessMode == RandomAccess) fptr = &random_write;

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

    MicroBenchmarkHarness::Init("DAX/ST", argc, argv);
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

    void *(*memcpyPtr)(void *, const void *, size_t) = memcpy;
    if (storeMode == NonTempStoreNoBarrier || storeMode == NonTempStoreAndBarrier) {
        memcpyPtr = pmem_memcpy_nodrain;
    }
    void (*barrierPtr)(void *, size_t) = barrier_empty;
    if (storeMode == StoreAndBarrier || storeMode == NonTempStoreAndBarrier) {
        barrierPtr = barrier_sfence;
    }
    else if (storeMode == StoreAndFlush) {
        barrierPtr = barrier_flush;
    }

    // Prepare configurations
    vector<ThreadArgs *> threadArgs;
    for (unsigned int i = 0; i < threadCount; i++) {
        ThreadArgs *t = new ThreadArgs;
        t->threadID = i;
        t->viewPtr = nvHeapPtr;
        t->viewLen = nvMapLen;
        t->seed = i;
        t->buffer = (uint64_t *)malloc(accessSize);
        t->memcpyPtr = memcpyPtr;
        t->barrierPtr = barrierPtr;
        t->nextBlockToWrite = 0;
        t->totalBlocks = nvMapLen / accessSize;
        threadArgs.push_back(t);
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
