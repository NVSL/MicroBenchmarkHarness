#ifndef HARNESS_BARRIER_INCLUDED
#define HARNESS_BARRIER_INCLUDED

#include<cassert>

#ifdef NO_BARRIERS

namespace nvsl {
     class Barrier {
	  int _numThreads;
	  volatile int _joined;
	  pthread_mutex_t _lock;
	  pthread_cond_t _cond;
     public:
	  Barrier(int count) : _numThreads(count), _joined(0) {
	       pthread_mutex_init(&_lock, NULL);
	       pthread_cond_init(&_cond, NULL);
	  }
	  ~Barrier() {
	       pthread_mutex_destroy(&_lock);
	       pthread_cond_destroy(&_cond);
	  }
	  void Join() {
	       pthread_mutex_lock(&_lock);
	       _joined++;
	       if (_joined == _numThreads) {
		    _joined = 0;
		    pthread_cond_broadcast(&_cond);
	       } else {
		    pthread_cond_wait(&_cond, &_lock);
	       }
	       pthread_mutex_unlock(&_lock);
	  }
     };

}

#undef PTHREAD_BARRIER_SERIAL_THREAD
#define PTHREAD_BARRIER_SERIAL_THREAD 1010
typedef nvsl::Barrier * pthread_barrier;
     
inline static int pthread_barrier_init(pthread_barrier * b, void *foo, int count) {
     *b = new nvsl::Barrier(count);
     assert(foo == NULL);
     return 0;
}
     
inline static int pthread_barrier_destroy(pthread_barrier *b) {
     delete *b;
     b = NULL;
     return 0;
}
     
inline static int pthread_barrier_wait(pthread_barrier *b) {
     (*b)->Join();
     return 0;
}
 


#else
namespace nvsl { 
    class Barrier {
	  pthread_barrier _barrier;
     public:
	  Barrier(int count) {
	       pthread_barrier_init(&_barrier, NULL, count);
	  }
	  ~Barrier() {
	       pthread_barrier_destroy(&_barrier);
	  }
	  void Join() {
	       pthread_barrier_wait(&_barrier);
	  }
     };
}
#endif
#endif
