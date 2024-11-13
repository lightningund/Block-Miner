all: chain.o csha256.o
	g++ chain.o csha256.o -g -std=c++20 -Wall -o chain.out

chain.o: chain.cpp
	g++ chain.cpp -o chain.o -c -Wall -std=c++20

csha256.o: csha256.cpp
	g++ csha256.cpp -o csha256.o -c -Wall -std=c++20

miner: kernel.o sha256.o miner.o
	nvcc miner.o csha256.o kernel.o sha256.o -o miner.out

kernel.o: kernel.cu
	nvcc kernel.cu -o kernel.o -dc

sha256.o: sha256.cu
	nvcc sha256.cu -o sha256.o -dc

miner.o: miner.cpp
	g++ miner.cpp -o miner.o -c -Wall -std=c++20