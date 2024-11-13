CPPFLAGS := -g -std=c++20 -Wall
CUDAFLAGS := --expt-relaxed-constexpr

all: chain.o csha256.o
	g++ $^ -o chain.out $(CPPFLAGS)

chain.o: chain.cpp
	g++ chain.cpp -o chain.o -c $(CPPFLAGS)

csha256.o: csha256.cpp
	g++ csha256.cpp -o csha256.o -c $(CPPFLAGS)

miner: miner.o kernel.o sha256.o
	nvcc $^ -o miner.out $(CUDAFLAGS)

dev.o: kernel.cu
	nvcc kernel.cu -o kernel.o -dc $(CUDAFLAGS)

sha256.o: sha256.cu
	nvcc sha256.cu -o sha256.o -dc $(CUDAFLAGS)

miner.o: miner.cpp
	g++ miner.cpp -o miner.o -c $(CPPFLAGS)