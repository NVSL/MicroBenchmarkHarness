#ifndef NVTM_HARNESS_INCLUDED
#define NVTM_HARNESS_INCLUDED

#include <stdio.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include "AtomicOps.hpp"
#include "HarnessBarrier.hpp"
#include "FastRand.hpp"
#include <assert.h>
#include <list>

namespace nvsl {
     // Making it a template lets us define everything in this header.
     // Otherwise, we'd need a .cpp for the static members, and it would be
     // pain to include it everywhere.
     template<class T>
     class _MicroBenchmarkHarness {
	  static size_t _footPrintB;
	  static double _runTimeSeconds;
	  static double _startTime;
	  static double _stopTime;
	  static volatile bool _finished;
	  static bool _timingIsSuspended;
	  static bool _opCountSet;
	  static unsigned int _threadCount;
	  static unsigned long long _operationCount;
	  static volatile long long _operationsCompleted;
	  static unsigned long long _operationsPerThread;
	  static bool _reload;
	  static bool _create;
	  static bool _hang;
	  static std::string _file;
	  static std::string _name;
	  static std::string _system;
	  
	  static Barrier *_startBarrier;
	  static Barrier *_endBarrier;
	  static Barrier *_runBarrier;
	  
	  static std::string _usage;

	  static double GetNow() {
	       struct timeval now;
	       gettimeofday(&now, NULL);
	       return  now.tv_sec + (now.tv_usec / 1000000.0);
	  }
	  
     public:
	  static void Init(const std::string & system, int & argc, char ** &argv) 
	  // Call this first thing in your program.  It will parse the standard
	  // commandline options and remove them from the argv.
	  {

	       assert(argc >= 1);
	       std::vector<char*> leftOvers;
	       leftOvers.push_back(argv[0]);

	       if (argc < 2) {
		    std::cerr << "Standard harness options : " << argv[0] << " " << _usage << "\n";
		    exit(1);
	       }

	       _name = argv[1];
	       _system = system;
	       bool durationSet = false;
	       int i=2;
	       while(i < argc) {
		    if (!std::strcmp(argv[i], "-tc"))
			 _threadCount = atoi(argv[++i]);
		    else if (!strcmp(argv[i], "-foot"))
			 _footPrintB = atoll(argv[++i])*1024*1024;
		    else if (!strcmp(argv[i], "-footMB"))
			 _footPrintB = atoll(argv[++i])*1024*1024;
		    else if (!strcmp(argv[i], "-footKB"))
			 _footPrintB = atoll(argv[++i])*1024;
		    else if (!strcmp(argv[i], "-footB"))
			 _footPrintB = atoll(argv[++i]);
		    else if (!strcmp(argv[i], "-rt")) {
			 durationSet = true;
			 _runTimeSeconds = atoi(argv[++i]);
		    } else if (!strcmp(argv[i], "-max")) {
			 durationSet = true;
			 _opCountSet = true;
			 _operationCount = atoi(argv[++i]);
			 
		    } else if (!strcmp(argv[i], "-file"))
			 _file = argv[++i];
		    else if (!strcmp(argv[i], "-hang"))
		         _hang = true;
		    else if (!strcmp(argv[i], "-nondet"))
			 srand(time(NULL));
		    else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
		         std::cout << "See http://github.com/NVSL/MicroBenchmarkHarness\n";
		    } else {
			 leftOvers.push_back(argv[i]);
		    }
		    i++;
	       }
	       

	       if (!durationSet) {
		    std::cerr << "-rt or -max must be specified\n";
		    std::cerr << argv[0] << _usage << "\n";
		    exit(-1);
	       }

	       if (_runTimeSeconds > 0 && _operationCount > 0) {
		    std::cerr << "only one of -rt or -max\n";
		    std::cerr << argv[0] << _usage << "\n";
		    exit(-1);
	       }


	       _operationsPerThread  = _operationCount/_threadCount;
	       _startTime = GetNow();

	       argc = leftOvers.size();
	       for(unsigned int i = 0; i< leftOvers.size(); i++) {
		    argv[i] = leftOvers[i];
	       }
	       argv[leftOvers.size()] = NULL;
	       

	       struct sigaction act;
	       bzero(&act, sizeof(act));
	       act.sa_handler = GracefulExit;
	       sigaction(SIGINT, &act, NULL);

	       StartTiming();
	  }

	  static const std::string & GetStandardOptionsUsage() {
	       return _usage;
	  }

	  static void GracefulExit(int s) {std::cerr << "Received signal " << s << " exiting.\n"; exit(1);}

	  static void SetRunTime(double t) {_runTimeSeconds = t;}
	  static void SetFootPrintMB(size_t size) {_footPrintB = size * 1024*1024;};
	  
	  inline static double GetAllowedRunTime()  {return _runTimeSeconds;}
	  inline static size_t GetFootPrintMB()  {return _footPrintB/1024/1024;}
	  inline static size_t GetFootPrintKB()  {return _footPrintB/1024;}
	  inline static size_t GetFootPrintBytes()  {return _footPrintB;}

	  inline static unsigned int GetThreadCount()  {return _threadCount;}
	  inline static unsigned long long GetOperationCount()  {return _operationCount;}
	  inline static unsigned long long GetOperationCountPerThread()  {return _operationsPerThread;}
	  inline static void CompletedOperation() {nvsl::atomic_increment(&_operationsCompleted);}
	  inline static unsigned long long GetCompletedOperations() {return _operationsCompleted;}
	  inline static void CompletedOperations(long long numOps) {nvsl::atomic_exchange_and_add(&_operationsCompleted, numOps);}
	  inline static const std::string & GetFileName()  {return _file;}
	  inline static double GetElapsedRunTime() {return _stopTime - _startTime;}


	  static void PrintResults(std::ostream & out = std::cout) {  
	       // Print out the timing and operation count results for the
	       // program in a standard format.
	       if (_stopTime == 0) {
		    StopTiming();
	       }
	       out << "Bench\tConfig\tRunTime\tOperations\tThreads\topsPerSec\n";
	       out <<     _system 
		   << "\t" << _name 
		   << "\t" << _stopTime - _startTime 
		   << "\t" << _operationsCompleted 
		   << "\t" << _threadCount 
		   << "\t" << (static_cast<float>(_operationsCompleted)/(_stopTime - _startTime))
		   << "\n";
	  }

     private:
	  static void StopSignal(int i) { // don't call this.
	       if (!_timingIsSuspended) {
		    _stopTime = GetNow();
		    _finished = true;	      
	       }
	  }
     public:
	  static void StartTiming() {  // Start the timed portion of the code.
	       // This is called automatically from
	       // Init(), but if there is a portion of
	       // the code you don't want to count in
	       // the timed portion, you could call
	       // SuspendTiming() and then call
	       // StartTiming() explicitly when the
	       // timed portion begins.
	       _startTime = GetNow();
	       //std::cerr << "Timing started\n";
	       struct sigaction act;
	       bzero(&act, sizeof(act));
	       act.sa_handler = StopSignal;
	       act.sa_flags = SA_RESETHAND;
	       sigaction(SIGALRM, &act, NULL);
	       _timingIsSuspended = false;
	       alarm(static_cast<unsigned int>(_runTimeSeconds));
	       volatile int temp = 1;
	       if (_hang) {
		    while(temp) {}
	       }
	  }

	  static void SuspendTiming() { // This suspends timing.  Call this
	       // after Init() if there is an
	       // initialization phase that should not
	       // be timed.
	       //std::cerr << "Suspending timing\n";
	       _timingIsSuspended = true;
	  }

	  static void StopTiming() {  // Call this when the timed portion of
	       // the program has ended.
	       _stopTime = GetNow();
	       //std::cerr << "Timing stopped\n";
	  }
	  
	  inline static bool isDone() {  // Check if the program should
	       // terminate: either 1) it has
	       // executed enough operationns or 2)
	       // it is out of time.
	       
	       if (_operationCount == 0 &&
		   _opCountSet) {
		    return true;
	       }
	       if (_operationCount == 0) {
		    return  _finished;
	       } else {
		    return static_cast<unsigned int>(_operationsCompleted) >= _operationCount;
	       }
	  }
	       
	  typedef std::vector<pthread_t*> ThreadVector;
	  static ThreadVector _threads;

	  template<class C>
	  static void StartThread(void *(*start_routine)(void*), C *arg) {
	       pthread_t * t = new pthread_t;
	       _threads.push_back(t);
	       pthread_create(t, NULL, start_routine, reinterpret_cast<void*>(arg));
	  }

	  static void WaitForThreads() {
	       for(ThreadVector::iterator i = _threads.begin();
		   i != _threads.end();
		   i++) {
		    pthread_join(**i, NULL);
	       }
	  }

	  typedef void (OpFunction)(int, void *, uint64_t &);

	  struct RunArgs {
	       RunArgs(OpFunction run , void *arg, uint i):
		    op_routine(run),
		    arg(arg),
		    id(i),
		    randSeed(rand()){ 
		    assert(randSeed != 0);
	       } // Avoid zero
	       OpFunction *op_routine;
	       void * arg;
	       const uint id;
	       uint64_t randSeed;

	  };

	  typedef std::list<RunArgs * > ArgsList;

	  static ArgsList _argsList;
	  
	  static void *GenericThread(void *a) {
	       RunArgs * args = reinterpret_cast< RunArgs * > (a);
	       unsigned int threadOps = GetOperationCountPerThread();
	       _startBarrier->Join();
	  
	       if (args->id == 0) {
		    StartTiming();
	       }
     
	       _runBarrier->Join();    
     
	       if (threadOps == 0) {     
		    while (!isDone()) {
			 args->op_routine(args->id, args->arg, args->randSeed);
			 threadOps++;
		    }     
	       } else {
		    for (unsigned int j = 0; j < threadOps; j++) {
			 args->op_routine(args->id, args->arg, args->randSeed);
		    }
	       }
     
	       CompletedOperations(threadOps);
	       _runBarrier->Join();
	       return  NULL;
	  }

	  static void RunOps(OpFunction * op_routine, void *arg) {
	       _startBarrier = new Barrier(_threadCount);
	       _endBarrier   = new Barrier(_threadCount);
	       _runBarrier   = new Barrier(_threadCount);
	       
	       for(unsigned int i= 0; i< _threadCount; i++) {
		    std::cerr << ".";
		    RunArgs *n = new RunArgs(op_routine, arg, i);
		    _argsList.push_back(n);
		    StartThread(GenericThread,reinterpret_cast<void*>(n));
	       }
	       
	       WaitForThreads();
	       
	       for(typename ArgsList::iterator i = _argsList.begin(); 
		   i != _argsList.end();
		   i++) {
		    delete *i;
	       }
	       _argsList.clear();

	       delete _startBarrier;
	       delete _endBarrier;
	       delete _runBarrier;
	  }
     };

     typedef _MicroBenchmarkHarness<int> MicroBenchmarkHarness;

     template<class C> 
     size_t _MicroBenchmarkHarness<C>::_footPrintB = 1*1024*1024;
     template<class C> 
     double _MicroBenchmarkHarness<C>::_runTimeSeconds = 0;
     template<class C> 
     double _MicroBenchmarkHarness<C>::_startTime = 0;
     template<class C> 
     double _MicroBenchmarkHarness<C>::_stopTime = 0;
     template<class C> 
     volatile bool _MicroBenchmarkHarness<C>::_finished = false;
     template<class C> 
     bool _MicroBenchmarkHarness<C>::_timingIsSuspended = false;
     template<class C> 
     unsigned int _MicroBenchmarkHarness<C>::_threadCount = 1;
     template<class C> 
     unsigned long long _MicroBenchmarkHarness<C>::_operationCount = 0;
     template<class C> 
     volatile long long _MicroBenchmarkHarness<C>::_operationsCompleted = 0;
     template<class C> 
     unsigned long long _MicroBenchmarkHarness<C>::_operationsPerThread= 0;
     template<class C>  
     bool _MicroBenchmarkHarness<C>::_reload = false;
     template<class C>  
     bool _MicroBenchmarkHarness<C>::_opCountSet = false;
     template<class C>
     bool _MicroBenchmarkHarness<C>::_create = false;
     template<class C>
     bool _MicroBenchmarkHarness<C>::_hang = false;
     template<class C>  
     std::string _MicroBenchmarkHarness<C>::_file;
     template<class C>  
     std::string _MicroBenchmarkHarness<C>::_name;
     template<class C>  
     std::string _MicroBenchmarkHarness<C>::_system;

     template<class C>  
     Barrier * _MicroBenchmarkHarness<C>::_startBarrier;
     template<class C>  
     Barrier * _MicroBenchmarkHarness<C>::_endBarrier;
     template<class C>  
     Barrier * _MicroBenchmarkHarness<C>::_runBarrier;
     template<class C>
     typename _MicroBenchmarkHarness<C>::ArgsList _MicroBenchmarkHarness<C>::_argsList;

     template<class C>
     typename _MicroBenchmarkHarness<C>::ThreadVector _MicroBenchmarkHarness<C>::_threads;
     template<class C>
     std::string _MicroBenchmarkHarness<C>::_usage = "<identifying string> [--help] [-rt <RunTime>|-max <MaxOps>] [-tc <#Threads>] [-footB <footprint B> | -footMB <footprint MB> |  -foot <FootprintMB> | -footKB <FootprintKB>]  [-file <backing file>] ";
}
#endif

//  Local Variables:
//  mode: c++
//  c-basic-offset: 5
//  End:
