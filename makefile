all:
	g++ chain.cpp -g -std=c++20 -Wall -o chain.out

miner:
	nvcc kernel.cu -o kernel.o -dc
	g++ miner.cpp -o miner.o -c -Wall -std=c++20
	nvcc miner.o kernel.o -o miner.out