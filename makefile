CPPC := clang++
CPPFLAGS := -std=c++20 -Wall
CUDAFLAGS := --expt-relaxed-constexpr -O2

CPPSRCS := $(wildcard *.cpp)
CPPOBJS := $(CPPSRCS:.cpp=.o)
CUDASRCS := $(wildcard *.cu)
CUDAOBJS := $(CUDASRCS:.cu=.o)

all: chain.o csha256.o
	$(CPPC) $^ -o chain.out $(CPPFLAGS)

miner: miner.o kernel.o sha256.o csha256.o
	nvcc $^ -o miner.out $(CUDAFLAGS)

$(CPPOBJS): %.o: %.cpp
	$(CPPC) $^ -o $@ -c $(CPPFLAGS)

$(CUDAOBJS): %.o: %.cu
	nvcc $^ -o $@ -dc $(CUDAFLAGS)