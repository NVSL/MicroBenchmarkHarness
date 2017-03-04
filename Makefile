TEST_SRCS=time_GSPS.cpp	time_random.cpp file_rd.cpp

TEST_EXES=$(TEST_SRCS:.cpp=.exe)

%.exe : %.cpp
	g++ -Wall -O3 -g -I.. -I/usr/local/include/ -I/root/Akshatha/boost_1_57_0 -lpthread -pthread -DNO_BARRIERS $< -o $@


.PHONY: default
default: $(TEST_EXES)

.PHONY: clean
clean:
	rm -rf $(TEST_EXES)
	rm -rf *.dSYM
