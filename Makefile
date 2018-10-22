CPP=g++
CC=gcc
COPTS?=-O4
CPPFLAGS?=-Wall $(COPTS) -I.. -I/usr/local/include/ -I/usr/local/boost
CPPFLAGS+=-pthread -g  -DNO_BARRIERS 
LDFLAGS?=-lpmem
LDFLAGS+=-lpthread -pthread

TEST_SRCS?=time_GSPS.cpp time_random.cpp file_rd.cpp file_wr.cpp file_ops.cpp dax_load.cpp dax_store.cpp

TEST_EXES=$(TEST_SRCS:.cpp=.exe)

TO_CLEAN=$(TEST_EXES)

.PHONY: default
default: $(TEST_EXES)

%.exe : %.cpp $(OBJS)
	$(CPP) $(CPPFLAGS) $^ -o $@ $(LDFLAGS)
	chmod u+x $@

%.o : %.cpp
	$(CPP) $(CPPFLAGS) $< -o $@ $(LDFLAGS)

%.o : %.c
	$(CC) $(CPPFLAGS) -c $< -o $@ 

.PHONY: default
default: $(TEST_EXES)

.PHONY: clean
clean:
	rm -rf $(TO_CLEAN)
	rm -rf *.dSYM
