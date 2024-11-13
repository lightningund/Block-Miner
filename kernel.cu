#include <string>
#include "kernel.cuh"
#include "sha256.cuh"

__global__
void hash_block(
	const char* last_hash,
	const char* input,
	size_t inputlen,
	hash_t& hash
) {
	kernel_sha256_hash(
		reinterpret_cast<const BYTE*>(input),
		inputlen,
		hash.data(),
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

	std::vector<char> last_vec{last_hash.begin(), last_hash.end()};
	hash_t hash;
	hash_block<<<1, 1>>>(last_vec.data(), input.data(), input.size(), hash);
	return hash;
}