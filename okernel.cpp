// Openmp CPU version

#include "kernel.cuh"
#include "osha.hpp"
#include "shared.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <omp.h>

using namespace std::chrono;
using timepoint = time_point<system_clock>;

static inline timepoint get_now() {
	return std::chrono::system_clock::now();
}

constexpr auto difficulty = 8;
constexpr auto nonce_max = 16;

constexpr auto test_loops = 56;

template<size_t len>
std::ostream& operator<<(std::ostream& os, const std::array<BYTE, len>& data) {
	for (auto byte : data) {
		os << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte;
	}
	return os;
}

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

void test_nonce(HashContext ctx, const size_t offset, size_t* golden, bool* found) {
	#pragma omp parallel for firstprivate(ctx) num_threads(test_loops)
	for (size_t i = 0; i < test_loops; ++i) {
		size_t nonce = i + offset * test_loops;
		ctx.update(nonce);

		if (ctx.test(difficulty)) {
			*found = true;
			*golden = nonce;
		}
	}
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
	size_t golden;
	bool found = false;
	size_t loops = idx * 0xFFFFFF;

	std::cout << "Finding nonce\n";

	timepoint start = get_now();

	while (found == false) {
		test_nonce(data->ctx, loops, &golden, &found);
		++loops;

		// Only run the io check every 4096 loops
		if ((loops & 0xFFF) == 0) {
			refresher();
		}
	}

	timepoint stop = get_now();
	int64_t dur = duration_cast<milliseconds>(stop - start).count();
	std::cout << std::dec;
	std::cout << "Finding the nonce took: " << dur << " ms\n";
	std::cout << "Loops: " << loops << "\n";
	std::cout << dur / loops << "ms/loop\n";

	std::array<char, nonce_max> nonce;
	for (int i = 0; i < nonce_max; ++i) {
		nonce[i] = 'A' + (golden & 0xF);
		golden >>= 4;
	}

	data->curr.nonce = std::string{nonce.data()};
	data->curr.nonce = data->curr.nonce.substr(0, nonce_max);
	std::cout << data->curr.nonce << "\n";
	hash_t hash = hash_block(data->last_hash, data->curr);
	data->curr.hash = hash_to_string(hash);
}