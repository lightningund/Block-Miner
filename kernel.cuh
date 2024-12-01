#pragma once

#include <array>
#include <string>
#include <vector>
#include <functional>

#include "shared.hpp"

using std::string;
using hash_t = std::array<BYTE, 32>;

struct FinderData;

class Finder {
	public:
		Finder(Block& block);
		~Finder();
		void find_nonce();
		void find_nonce(size_t idx);
		void find_nonce(const std::function<void(void)> refresher, size_t idx);
		void set_last_hash(const string last_hash);
		void set_block(Block& block);

	private:
		FinderData* data;
};

hash_t hash_block(const string& last_hash, const Block& block);
