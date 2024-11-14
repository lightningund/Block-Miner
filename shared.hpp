#pragma once

#include <vector>
#include <string>
using std::string;

struct Block {
	string minedBy;
	std::vector<string> messages; // Each message is <=20 characters, max 10 messages
	string nonce; // Must be under 40 characters
	size_t height;
	size_t timestamp;
	string hash;
};