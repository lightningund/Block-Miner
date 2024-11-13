all:
	g++ chain.cpp -g -std=c++20 -Wall -o chain.out

miner: kernel.o sha256.o miner.o
	nvcc miner.o kernel.o sha256.o -o miner.out

kernel.o: kernel.cu
	nvcc kernel.cu -o kernel.o -dc

sha256.o: sha256.cu
	nvcc sha256.cu -o sha256.o -dc

miner.o: miner.cpp
	g++ miner.cpp -o miner.o -c -Wall -std=c++20