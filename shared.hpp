#pragma once

#include <vector>
#include <string>
using std::string;

#define LOG_ERROR(msg) std::cerr << "\033[31m" << msg << "\033[0m\n"

struct Block {
	string minedBy;
	std::vector<string> messages; // Each message is <=20 characters, max 10 messages
	string nonce; // Must be under 40 characters
	size_t height;
	size_t timestamp;
	string hash;
};