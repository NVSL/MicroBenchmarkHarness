CC=g++
CFLAGS=-Wall -O3 -g -I.. -I/usr/local/include/ -I/usr/local/boost -DNO_BARRIERS
LDFLAGS=-lpthread -pthread -lpmem

TEST_SRCS=time_GSPS.cpp	time_random.cpp file_rd.cpp file_wr.cpp file_ops.cpp dax_load.cpp dax_store.cpp

TEST_EXES=$(TEST_SRCS:.cpp=.exe)

%.exe : %.cpp
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

.PHONY: default
default: $(TEST_EXES)

.PHONY: clean
clean:
	rm -rf $(TEST_EXES)
	rm -rf *.dSYM
