CPPC := g++
MSVCC := cl
CUDAC := nvcc
CPPFLAGS := -std=c++23 -Wall -O3 -Wno-sign-compare
MSVCFLAGS := /std:c++latest /EHsc
CUDAFLAGS := --expt-relaxed-constexpr -O3

CPPSRCS := $(wildcard *.cpp)
CPPOBJS := $(CPPSRCS:.cpp=.o)
MSVCOBJS := $(CPPSRCS:.cpp=.obj)
CUDASRCS := $(wildcard *.cu)
CUDAOBJS := $(CUDASRCS:.cu=.o)

all: chain miner cpu_miner

chain: chain.o csha256.o sweatshop.o
	$(CPPC) $^ -o chain.out $(CPPFLAGS)

winchain: MSVCFLAGS += /ID:/CS_ALIAS/C++/@LIBS/MSVC/x64/boost_1_91_0/
winchain: chain.obj csha256.obj sweatshop.obj
	$(MSVCC) $^ $(MSVCFLAGS) /Fe:chain.exe

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

$(MSVCOBJS): %.obj: %.cpp
	$(MSVCC) $^ /c /Fo:$@ $(MSVCFLAGS)

$(CUDAOBJS): %.o: %.cu
	$(CUDAC) $^ -o $@ -dc $(CUDAFLAGS)
