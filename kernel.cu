#include <string>
#include <iostream>
#include <iomanip>
#include "kernel.cuh"
#include "sha256.cuh"

constexpr auto difficulty = 9;
constexpr auto nonce_max = 16;

// Wrapper for managed memory objects
template <typename T>
struct Managed {
	T* raw;
	size_t size;

	Managed() : size{sizeof(T)} {
		cudaMallocManaged(&raw, size);
	}

	// Takes the size as a number of bytes
	Managed(size_t size) : size{size} {
		cudaMallocManaged(&raw, size);
	}

	~Managed() {
		cudaFree(raw);
	}

	void operator=(const T* ptr) {
		cudaMemcpy(raw, ptr, size, cudaMemcpyHostToDevice);
	}

	operator T*() {
		return raw;
	}
};

template<size_t len>
std::ostream& operator<<(std::ostream& os, const std::array<BYTE, len>& data) {
	for (auto byte : data) {
		os << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte;
	}
	return os;
}

__global__
void hash_block(const BYTE* input, size_t inputlen, hash_t* hash) {
	HashContext ctx{};
	ctx.update(input, inputlen);
	ctx.digest(hash->data());
}

// __global__
// void hash_block(const BYTE* input, size_t inputlen, hash_t* hash) {
// 	HashContext ctx{};
// 	for (int i = 0; i < inputlen; i += 10) {
// 		int len = ((i + 10) >= inputlen) ? inputlen - i : 10;
// 		ctx.update(&input[i], len);
// 	}
// 	ctx.digest(hash->data());
// }

hash_t hash_block(const string& last_hash, const Block& block) {
	string input = last_hash;
	// input += "Ben's GPU";
	input += block.minedBy;

	for (auto&& msg : block.messages) {
		input += msg;
	}

	uint64_t casted_stamp = static_cast<uint64_t>(block.timestamp);
	char* stamp_chars = reinterpret_cast<char*>(&casted_stamp);
	for (int i = 7; i >= 0; --i) {
		input += stamp_chars[i];
	}
	input += block.nonce;

	Managed<BYTE> dev_input{input.size()};
	dev_input = reinterpret_cast<const BYTE*>(input.c_str());
	Managed<hash_t> dev_hash{};
	hash_block<<<1, 1>>>(dev_input.raw, input.size(), dev_hash.raw);
	cudaDeviceSynchronize();

	hash_t hash;
	cudaMemcpy(&hash, dev_hash.raw, sizeof(hash_t), cudaMemcpyDeviceToHost);

	return hash;
}

string hash_to_string(const hash_t& hash) {
	char buf[2 * HashContext::DIGEST_SIZE + 1];
	buf[2 * HashContext::DIGEST_SIZE] = 0;

	for (int i = 0; i < HashContext::DIGEST_SIZE; i++) {
		sprintf(buf + i * 2, "%02x", hash[i]);
	}

	return string{buf};
}

__global__
void test_nonce(
	HashContext ctx,
	size_t offset,
	size_t* golden,
	bool* found
) {
	size_t thread = blockIdx.x * blockDim.x + threadIdx.x + offset * gridDim.x * blockDim.x;
	ctx.update(thread);

	if (!ctx.test(difficulty)) return;

	*found = true;
	*golden = thread;
}

__global__
void setup(const BYTE input[], size_t input_len, size_t* golden) {
	HashContext ctx{};
	ctx.update(input, input_len);
	bool* found = (bool*)malloc(sizeof(bool));
	*found = false;
	size_t loops = 0;
	while (*found == false) {
		test_nonce<<<512, 512>>>(ctx, loops, golden, found);
		++loops;
	}
	free(found);
	printf("Loops: %d\n", loops);
}

void find_nonce(const string& last_hash, Block& block) {
	cudaEvent_t start, stop;
	cudaEventCreate(&start);
	cudaEventCreate(&stop);
	cudaEventRecord(start);
	string input = last_hash;
	input += block.minedBy;

	for (auto&& msg : block.messages) {
		input += msg;
	}

	uint64_t casted_stamp = static_cast<uint64_t>(block.timestamp);
	char* stamp_chars = reinterpret_cast<char*>(&casted_stamp);
	for (int i = 7; i >= 0; --i) {
		input += stamp_chars[i];
	}

	Managed<BYTE> dev_input{input.size()};
	dev_input = reinterpret_cast<const BYTE*>(input.c_str());
	Managed<size_t> dev_golden{};

	setup<<<1, 1>>>(dev_input, input.size(), dev_golden);
	cudaDeviceSynchronize();
	size_t golden;
	cudaMemcpy(&golden, dev_golden, sizeof(size_t), cudaMemcpyDeviceToHost);

	std::array<BYTE, nonce_max> nonce;
	for (int i = 0; i < nonce_max; ++i) {
		nonce[i] = 'A' + (golden & 0xF);
		golden >>= 4;
	}

	std::cout << nonce << "\n";
	std::cout << std::dec << nonce.size() << "\n";

	block.nonce = std::string{reinterpret_cast<char*>(nonce.data())};
	block.nonce = block.nonce.substr(0, nonce_max);
	std::cout << block.nonce << "\n";
	std::cout << std::dec << block.nonce.size() << "\n";
	hash_t hash = hash_block(last_hash, block);
	block.hash = hash_to_string(hash);
	cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    float time;
    cudaEventElapsedTime(&time, start, stop);
	std::cout << "Finding the nonce took: " << time << " ms\n";
}