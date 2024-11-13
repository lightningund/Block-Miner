#include <string>
#include <iostream>
#include <iomanip>
#include "kernel.cuh"
#include "sha256.cuh"

constexpr auto difficulty = 5;
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

	Managed<BYTE> dev_input{input.size()};
	dev_input = reinterpret_cast<const BYTE*>(input.c_str());
	Managed<hash_t> dev_hash{};
	hash_block<<<1, 1>>>(dev_input.raw, input.size(), dev_hash.raw);

	hash_t hash;
	cudaMemcpy(&hash, dev_hash.raw, sizeof(hash_t), cudaMemcpyDeviceToHost);

	return hash;
}

__global__
void test_nonce(
	HashContext ctx,
	size_t offset,
	BYTE golden[],
	hash_t* hash,
	bool* found
) {
	size_t thread = blockIdx.x * blockDim.x + threadIdx.x + offset * gridDim.x * blockDim.x;
	BYTE nonce[nonce_max];
	for (int i = 0; i < nonce_max; ++i) {
		nonce[i] = (BYTE)thread;
		thread >>= 16;
	}
	ctx.update(nonce, nonce_max);
	hash_t temp;
	ctx.digest(temp.data());
	for (int i = 0; i < difficulty / 2; ++i) {
		if (temp[HashContext::DIGEST_SIZE - i - 1] != 0) return;
	}

	if (difficulty % 2 == 1) {
		size_t idx = HashContext::DIGEST_SIZE - (difficulty / 2);
		if ((temp[idx] & 0xF) != 0) return;
	}

	*found = true;
	memcpy(golden, nonce, sizeof(nonce));
	memcpy(hash, &temp, sizeof(temp));
}

__global__
void setup(const BYTE* input, size_t input_len, BYTE nonce[], hash_t* hash) {
	HashContext ctx{};
	ctx.update(input, input_len);
	bool found = false;
	size_t loops = 0;
	while (!found) {
		test_nonce<<<256, 256>>>(ctx, loops, nonce, hash, &found);
		++loops;
	}
}

void find_nonce(const string& last_hash, G_Block& block) {
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
	Managed<hash_t> dev_hash{};
	Managed<std::array<BYTE, nonce_max>> dev_nonce{};

	setup<<<1, 1>>>(dev_input.raw, input.size(), dev_nonce.raw->data(), dev_hash.raw);
	std::array<BYTE, nonce_max> nonce;
	cudaMemcpy(&nonce, dev_nonce.raw, sizeof(nonce), cudaMemcpyDeviceToHost);
	hash_t hash;
	cudaMemcpy(&hash, dev_hash.raw, sizeof(hash), cudaMemcpyDeviceToHost);
	std::cout << nonce << "\n";
	std::cout << hash << "\n";

	char buf[2 * nonce_max + 1];
	buf[2 * nonce_max] = 0;

	for (int i = 0; i < nonce_max; i++) {
		sprintf(buf + i * 2, "%02x", nonce[i]);
	}

	block.nonce = std::string{buf};
	hash_t real_hash = hash_block(last_hash, block);
	std::cout << real_hash << "\n";
}