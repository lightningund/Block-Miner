CPPC := g++
CPPFLAGS := -std=c++23 -Wall
CUDAFLAGS := --expt-relaxed-constexpr

CPPSRCS := $(wildcard *.cpp)
CPPOBJS := $(CPPSRCS:.cpp=.o)
CUDASRCS := $(wildcard *.cu)
CUDAOBJS := $(CUDASRCS:.cu=.o)

chain: chain.o csha256.o sweatshop.o
	$(CPPC) $^ -o chain.out $(CPPFLAGS)

evil_chain: CPPFLAGS += -DEVIL_MODE
evil_chain: chain.o csha256.o sweatshop.o
	$(CPPC) $^ -o chain.out $(CPPFLAGS)

comp_chain: CPPFLAGS += -DCOMP_CHAIN -O2
comp_chain: chain.o csha256.o sweatshop.o
	$(CPPC) $^ -o comp_chain.out $(CPPFLAGS)

miner: CUDAFLAGS += -O2
miner: miner.o kernel.o sha256.o csha256.o
	nvcc $^ -o miner.out $(CUDAFLAGS)

comp_miner: CPPFLAGS += -DCOMP_CHAIN -O2
comp_miner: CUDAFLAGS += -DCOMP_CHAIN -O2
comp_miner: miner.o kernel.o sha256.o csha256.o
	nvcc $^ -o comp_miner.out $(CUDAFLAGS)

cpu_miner: CPPFLAGS += -O2 -fopenmp
cpu_miner: miner.o okernel.o osha.o csha256.o
	$(CPPC) $^ -o cpu_miner.out $(CPPFLAGS)

profiler: CPPFLAGS += -g
profiler: CUDAFLAGS += -g
profiler: profile_miner.o kernel.o sha256.o
	nvcc $^ -o profiler.out $(CUDAFLAGS)

$(CPPOBJS): %.o: %.cpp
	$(CPPC) $^ -o $@ -c $(CPPFLAGS)

$(CUDAOBJS): %.o: %.cu
	nvcc $^ -o $@ -dc $(CUDAFLAGS)
