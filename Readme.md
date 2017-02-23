MicroBenchmarkHarness
=====================

Purpose:  A harness meant for measuring the performance of simple operations (i.e., microbenchmarks).  

Goals:

1.  Uniform output format to make graphing easy.
2.  Accurate measurements of very fast operations.
3.  A variety of useful general-purpose options.
4.  Handle some tricky stuff like signals and threads nicely.

Key Concepts
============

The *function under test (FUT)* is the function you want to measure the performance of.

The `MicroBenchmarkHarness` singleton object is key object your test needs to interact with.  It provides the following services:

1.  Parses standard commandline arguments (via `Init()`).
2.  Process access to the values of standard command line options (e.g., `GetThreadCount()`)
3.  Provides a facility for starting threads and waiting for them to complete (`StartThread()` and `WaitForThreads()').
4.  Invokes the FUT repeatedly for period of time or number of invocations (`RunOps()`)
5.  Prints nicely formated results (`PrintResuts()')

Standard Command Line Arguments
===============================

`your_executable identifying_string [--help] [-rt <RunTime>|-max <MaxOps>] [-tc <#Threads>] [-footB <footprint B> | -footMB <footprint MB> |  -foot <FootprintMB> | -footKB <FootprintKB>]  [-file <backing file>]`

* `<identifying string>` (Required):  An an identifying string that will get printed with the output.
* `-rt <RunTime>|-max <MaxOps>` (Required): Set the number of ops to run or the amount of time to run for.
* `-tc <#Threads>`:  How many threads?
* `-footB <footprint B> | -footMB <footprint MB> |  -foot <FootprintMB> | -footKB <FootprintKB>`:  Set the footprint of the microbenchmark (e.g., for a random memory accesses, this could be the amount of memory to use).  Defaults to 1MB.
* `-file <backing file>`:  File or directory to run the benchmark on/in/about.  Interpretation is benchmark-specific.  Defaults to "".
* `--help`:  Get some help

Compiling
=========

You need to add this directory to `-I` for `g++`.  If your platform`s pthreads doesn't include a barrier (like on Macs), you can pass -DNO_BARRIERS.

See `Makefile` for details.

To build examples do `make`



RandLFSR()
==========
`RandLFSR()` in `FastRand.hpp` is a very fast 64-bit linear-feedback shift register (LFSR)-based pseudo-random number generator.  It solves the problem that 1) we would like to be able to run random tests and 2) good random number generators are often slower than the operations we want to measure, so we end up measuring the random number generator instead of the operation.   `FastRand()` fixes this by being  "random enough" for most purposes and extremely fast (3.2ns/call on my laptop, although you can  measure it yourself `time_random.cpp`)

`RandLFSR()` takes a pointer to it's 'seed' as an argument, sets it to the next random value and returns the value.  `MicroBechmrakHarness()` passes the current thread's seed as an argument to the FUT.

Output
======

One of the most useful features of `MicroBenchmarkHarness` is that it produces output that easy to graph.  In particular, it ensures that the output from all our microbenchmarks is in the same format so the same graphing scripts work on all of it.  (I'm not kidding, this a big reason that we wrote it).

Here's an example

| Bench   | Config	| RunTime	  | Operations	|  Threads | opsPerSec  |
|---------|-------------|-----------------|-------------|----------|------------|
| RandLFSR|	1MB	| 2.00364	  | 611335565	| 1	   | 3.05112e+08|


* `Bench` is the name of the benchmarks.  It comes from the first argument to `Init()`
* `Config` is the first command line argument to the benchmark executable.  It is useful to include descriptive information about this run of the benchmark. In this case it tells us that it ran with a 1MB footprint.  This string should unique all the runs of this microbenchmark an the string should be structured so it's easy to grep/search for what you want.
* `Runtime` is the runtime in seconds.
* `Operations` is the total numbers of times the FUT ran.
* `Threads` is the number of threads
* `OpsPerSec` is the number of times FUT executed per second across all threads.



Simple Example
==============

`time_random.cpp` contains an example of the simplest usage suitable for measuring the performance of simple operation that doesn't require complex setup ahead of time.  Used this way, all you have to implement a single function that performs the operation you want to measure.  See comments for details.

Complex Example
===============

`time_GSPS.cpp` is a complex example that shows how to add your own command line options, create threads yourself, and interact with then MicroBenchmarkHarness singleton object.

See comments for details.


