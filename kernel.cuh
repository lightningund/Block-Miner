#pragma once

#include <array>
#include <string>
#include <vector>

#include "config.h"

using std::string;
using hash_t = std::array<BYTE, 32>;

struct G_Block {
	string minedBy;
	std::vector<string> messages; // Each message is <=20 characters, max 10 messages
	string nonce; // Must be under 40 characters
	size_t height;
	size_t timestamp;
	string hash;
};

hash_t hash_block(const string& last_hash, const G_Block& block);