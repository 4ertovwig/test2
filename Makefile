CXX=g++
CFLAGS=-c -pipe -std=c++11 -O2 -g -Wall -W -I.

all : naive_external_sort

naive_external_sort : main.o
	$(CXX) -o naive_external_sort main.o -pthread

main.o: main.cpp
	$(CXX) $(CFLAGS) -o main.o main.cpp

clean:
	rm -rf naive_external_sort *.dat *.o
