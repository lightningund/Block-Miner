#pragma once
#include <array>
#include <vector>
#include <string>
#include <iostream>

using std::string;

using BYTE = unsigned char;
using WORD = unsigned int;

template <typename... Args>
inline void log_err(Args... args) {
	((std::cerr << "\033[31m") << ... << args) << "\033[0m\n";
}

template <typename... Args>
inline void log_warn(Args... args) {
	((std::cerr << "\033[33m") << ... << args) << "\033[0m\n";
}

template<size_t len>
std::ostream& operator<<(std::ostream& os, const std::array<BYTE, len>& data) {
	for (auto byte : data) {
		os << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte;
	}
	return os;
}

struct Block {
	string minedBy;
	std::vector<string> messages; // Each message is <=20 characters, max 10 messages
	string nonce; // Must be under 40 characters
	long height = -1;
	size_t timestamp;
	string hash;
};
