CPPC := g++
CUDAC := nvcc
CPPFLAGS := -std=c++23 -Wall -O3
CUDAFLAGS := --expt-relaxed-constexpr -O3

CPPSRCS := $(wildcard *.cpp)
CPPOBJS := $(CPPSRCS:.cpp=.o)
CUDASRCS := $(wildcard *.cu)
CUDAOBJS := $(CUDASRCS:.cu=.o)

all: chain miner cpu_miner

chain: chain.o csha256.o sweatshop.o
	$(CPPC) $^ -o chain.out $(CPPFLAGS)

miner_demo: CPPFLAGS += -DSTANDALONE
miner_demo: chain.o csha256.o sweatshop.o
	$(CPPC) $^ -o stand_chain.out $(CPPFLAGS)

miner: miner.o kernel.o sha256.o csha256.o
	$(CUDAC) $^ -o miner.out $(CUDAFLAGS)

cpu_miner: CPPFLAGS += -fopenmp
cpu_miner: miner.o okernel.o osha.o csha256.o
	$(CPPC) $^ -o cpu_miner.out $(CPPFLAGS)

profiler: CPPFLAGS += -g
profiler: CUDAFLAGS += -g
profiler: profile_miner.o kernel.o sha256.o
	$(CUDAC) $^ -o profiler.out $(CUDAFLAGS)

$(CPPOBJS): %.o: %.cpp
	$(CPPC) $^ -o $@ -c $(CPPFLAGS)

$(CUDAOBJS): %.o: %.cu
	$(CUDAC) $^ -o $@ -dc $(CUDAFLAGS)
