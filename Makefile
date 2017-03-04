TEST_SRCS=time_GSPS.cpp	time_random.cpp read.cpp

TEST_EXES=$(TEST_SRCS:.cpp=.exe)

%.exe : %.cpp
	g++ -Wall -O3 -g -I.. -I/usr/local/include/ -lpthread -DNO_BARRIERS $< -o $@


.PHONY: default
default: $(TEST_EXES)

.PHONY: clean
clean:
	rm -rf $(TEST_EXES)
	rm -rf *.dSYM
