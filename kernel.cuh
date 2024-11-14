#pragma once

#include <array>
#include <string>
#include <vector>

#include "shared.hpp"

#include "config.h"

using std::string;
using hash_t = std::array<BYTE, 32>;

hash_t hash_block(const string& last_hash, const Block& block);

void find_nonce(const string& last_hash, Block& block);