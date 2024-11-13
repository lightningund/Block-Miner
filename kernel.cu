#include <string>
#include <iostream>
#include <iomanip>
#include "kernel.cuh"
#include "sha256.cuh"

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
};

__global__
void hash_block(
	const char* input,
	size_t inputlen,
	hash_t* hash
) {
	kernel_sha256_hash<<<1,1>>>(
		reinterpret_cast<const BYTE*>(input),
		inputlen,
		hash->data(),
		1
	);
}

hash_t hash_block(const string& last_hash, const G_Block& block) {
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

	Managed<char> dev_input{input.size()};
	dev_input = input.c_str();
	Managed<hash_t> dev_hash{};
	hash_block<<<1, 1>>>(dev_input.raw, input.size(), dev_hash.raw);

	hash_t hash;
	cudaMemcpy(&hash, dev_hash.raw, sizeof(hash_t), cudaMemcpyDeviceToHost);

	return hash;
}

__device__
void test_nonce(
	HashContext ctx,
	const BYTE* nonce,
	size_t nonce_len,
	hash_t* hash
) {
	ctx.update(nonce, nonce_len);
	ctx.digest(hash->data());
}

__global__
void test(
	const BYTE* input,
	size_t input_len,
	hash_t* hash_a,
	hash_t* hash_b
) {
	HashContext ctx{};
	ctx.update(input, input_len);
	test_nonce(ctx, "NONSENSE", 8, hash_a);
	test_nonce(ctx, "663135608617883", 15, hash_b);
}

void find_nonce(const string& last_hash, const G_Block& block) {
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

	Managed<BYTE> dev_input{input.size()};
	dev_input = reinterpret_cast<const BYTE*>(input.c_str());
	Managed<hash_t> dev_hash_a{};
	Managed<hash_t> dev_hash_b{};
	test<<<1, 1>>>(dev_input.raw, input.size(), dev_hash_a.raw, dev_hash_b.raw);

	hash_t hash;
	cudaMemcpy(&hash, dev_hash_a.raw, sizeof(hash), cudaMemcpyDeviceToHost);
	for (auto byte : hash) {
		std::cout << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte;
	}
	std::cout << "\n";
	cudaMemcpy(&hash, dev_hash_b.raw, sizeof(hash), cudaMemcpyDeviceToHost);
	for (auto byte : hash) {
		std::cout << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte;
	}
	std::cout << "\n";
}