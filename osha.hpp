#pragma once
#include <array>
#include <cstddef>

using BYTE = unsigned char;
using WORD = unsigned int;

struct HashContext {
	static constexpr size_t DIGEST_SIZE = 32;

	HashContext();

	void update(const BYTE incoming[], size_t len);
	void update(const char incoming[], size_t len);
	void update(size_t offset);
	void digest(BYTE hash[]);
	bool test(size_t difficulty);

private:
	std::array<BYTE, 64> data;
	WORD datalen;
	unsigned long long bitlen;
	WORD state[8];

	void transform();
};
