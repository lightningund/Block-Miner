#include <string>
#include <iostream>
#include <iomanip>
#include "kernel.cuh"
#include "sha256.cuh"

#ifndef COMP_CHAIN
constexpr auto difficulty = 6;
#else
constexpr auto difficulty = 9;
#endif
constexpr auto nonce_max = 16;

// Wrapper for managed memory objects
template <typename T>
struct Managed {
	T* raw;
	size_t size;

	Managed() : Managed{sizeof(T)} {}

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

	void operator=(const T dat) {
		cudaMemcpy(raw, &dat, size, cudaMemcpyHostToDevice);
	}

	operator T*() {
		return raw;
	}

	operator T() const {
		T dat;
		cudaMemcpy(&dat, raw, sizeof(T), cudaMemcpyDeviceToHost);
		return dat;
	}

	T get() const {
		T dat;
		cudaMemcpy(&dat, raw, sizeof(T), cudaMemcpyDeviceToHost);
		return dat;
	}
};

template<size_t len>
std::ostream& operator<<(std::ostream& os, const std::array<BYTE, len>& data) {
	for (auto byte : data) {
		os << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte;
	}
	return os;
}

hash_t hash_block(const string& last_hash, const Block& block) {
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
	input += block.nonce;

	hash_t hash;
	HashContext ctx{};
	ctx.update(input.c_str(), input.size());
	ctx.digest(hash.data());

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
	const uint64_t offset,
	uint64_t* golden,
	bool* found
) {
	uint64_t thread = blockIdx.x * blockDim.x + threadIdx.x + offset * gridDim.x * blockDim.x;
	ctx.update(thread);

	if (!ctx.test(difficulty)) return;

	*found = true;
	*golden = thread;
}

struct FinderData {
	Block& curr;
	string last_hash;
	HashContext ctx;
};

Finder::Finder(Block& block) {
	data = new FinderData{block, "", {}};
}

Finder::~Finder() {
	delete data;
}

void Finder::set_block(Block& block) {
	data->curr = block;
}

void Finder::set_last_hash(const string last_hash) {
	// Skip if it's the same, unless it's blank, because then it's probably the first call
	if (data->last_hash == last_hash && last_hash != "") return;
	data->last_hash = last_hash;
	data->ctx = HashContext{};
	string input = last_hash;
	input += data->curr.minedBy;

	for (auto&& msg : data->curr.messages) {
		input += msg;
	}

	uint64_t casted_stamp = static_cast<uint64_t>(data->curr.timestamp);
	char* stamp_chars = reinterpret_cast<char*>(&casted_stamp);
	for (int i = 7; i >= 0; --i) {
		input += stamp_chars[i];
	}

	data->ctx.update(input.c_str(), input.size());
}

void Finder::find_nonce() {
	find_nonce([](){}, 0);
}

void Finder::find_nonce(size_t idx) {
	find_nonce([](){}, idx);
}

void Finder::find_nonce(const std::function<void(void)> refresher, size_t idx) {
	Managed<bool> dev_found{};
	dev_found = false;
	Managed<uint64_t> dev_golden{};
	uint64_t loops = idx * 0xFFFFFF; // Just so all the miners aren't checking the same things

	cudaEvent_t start, stop;
	cudaEventCreate(&start);
	cudaEventCreate(&stop);
	cudaEventRecord(start);
	while (!dev_found.get()) {
		++loops;
		test_nonce<<<1024, 512>>>(data->ctx, loops, dev_golden, dev_found);
		cudaDeviceSynchronize();
		cudaDeviceSynchronize();

		// Only run the io check every 4096 loops
		if ((loops & 0xFFF) == 0) {
			refresher();
		}
	}
	cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    float time;
	std::cout << std::dec;
    cudaEventElapsedTime(&time, start, stop);
	std::cout << "Finding the nonce took: " << time << " ms\n";
	std::cout << "Loops: " << loops << "\n";
	std::cout << time / loops << "ms/loop\n";

	uint64_t golden = dev_golden;

	std::array<BYTE, nonce_max> nonce;
	for (int i = 0; i < nonce_max; ++i) {
		nonce[i] = 'A' + (golden & 0xF);
		golden >>= 4;
	}

	data->curr.nonce = std::string{reinterpret_cast<char*>(nonce.data())};
	data->curr.nonce = data->curr.nonce.substr(0, nonce_max);
	std::cout << data->curr.nonce << "\n";
	hash_t hash = hash_block(data->last_hash, data->curr);
	data->curr.hash = hash_to_string(hash);
}
