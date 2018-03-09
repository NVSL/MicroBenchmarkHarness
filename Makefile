TEST_SRCS=time_GSPS.cpp	time_random.cpp file_rd.cpp file_wr.cpp file_ops.cpp

TEST_EXES=$(TEST_SRCS:.cpp=.exe)

%.exe : %.cpp
	g++ -Wall -O3 -g -I.. -I/usr/local/include/ -I/usr/local/boost -lpthread -pthread -DNO_BARRIERS $< -o $@


.PHONY: default
default: $(TEST_EXES)

.PHONY: clean
clean:
	rm -rf $(TEST_EXES)
	rm -rf *.dSYM
