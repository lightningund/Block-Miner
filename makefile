CPPFLAGS := -g -std=c++20 -Wall
CUDAFLAGS := --expt-relaxed-constexpr

CPPSRCS := $(wildcard *.cpp)
CPPOBJS := $(CPPSRCS:.cpp=.o)
CUDASRCS := $(wildcard *.cu)
CUDAOBJS := $(CUDASRCS:.cu=.o)

all: chain.o csha256.o
	g++ $^ -o chain.out $(CPPFLAGS)

miner: miner.o kernel.o sha256.o
	nvcc $^ -o miner.out $(CUDAFLAGS)

$(CPPOBJS): %.o: %.cpp
	g++ $^ -o $@ -c $(CPPFLAGS)

$(CUDAOBJS): %.o: %.cu
	nvcc $^ -o $@ -dc $(CUDAFLAGS)