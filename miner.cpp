#include <iostream>
#include <array>

#include "csha256.hpp"

#include "types.hpp"
#include "helpers.hpp"

#include "json.hpp"
using json = nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Block, minedBy, messages, nonce, height, hash, timestamp)

// For intellisense
#include <boost/asio.hpp>
using boost::asio::ip::tcp;

host_t my_host = "127.0.0.1";
port_t my_port = 50002;
name_t my_name = "Ben's GPU";

boost::asio::io_context io_ctxt{};

#include "kernel.cuh"

string host_hash_block(string last_hash, Block block) {
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

	std::cout << "Hash Input: " << input << "\n";
	std::cout << "Input Length: " << input.size() << "\n";
	string hash = sha256(input);
	std::cout << "Hash: " << hash << "\n";
	std::cout << "Target Hash: " << block.hash << "\n";
	return hash;
}

// Tests the hash on the very first block
void test_hash() {
	Block test_block{
		.minedBy = "Prof!",
		.messages = {"Keep it", "simple.", "Veni", "vidi", "vici"},
		.nonce = "663135608617883",
		.height = 0,
		.timestamp = 1730910874,
		.hash = "75977fa09516d028befa0695e16c93be20271b66630236d38718e35700000000"
	};

	auto hash = hash_block("", test_block);
	string hash_str;
	for (auto byte : hash) {
		std::cout << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte;
	}

	std::cout << "\n" << test_block.hash << "\n";

	find_nonce("", test_block);

	host_hash_block("", test_block);
}

int main(int argc, char* argv[]) {
	test_hash();

	if (argc < 2) {
		std::cerr << "Please Give me a host idk what to do\n";
		return -1;
	}

	tcp::resolver resolver{io_ctxt};
	auto points = resolver.resolve(argv[1], "50001");
	tcp::socket chain{io_ctxt};
	boost::asio::connect(chain, points);

	std::array<char, 1024> buf;
	boost::system::error_code err;

	size_t len = chain.read_some(boost::asio::buffer(buf), err);

	string resp{buf.data()};
	resp = resp.substr(0, len);

	std::cout << resp << "\n";

	string msg = "Sup hoe";
	chain.send(boost::asio::buffer(msg));

	std::cout << "Waiting for last hash\n";
	len = chain.read_some(boost::asio::buffer(buf), err);

	string last_hash{buf.data()};
	last_hash = last_hash.substr(0, len);

	std::cout << last_hash << "\n";

	Block curr_block{
		.minedBy = "Ben's GPU",
		.messages = {
			"According to all",
			"known laws of",
			"aviation, there is",
			"no way a bee should",
			"be able to fly. Its",
			"wings are too small",
			"to get its fat",
			"little body off the",
			"ground. The bee, of",
			"course, flies"
		},
		.timestamp = 1731520534
	};

	while (true) {
		find_nonce(last_hash, curr_block);
		last_hash = curr_block.hash;
		std::cout << last_hash << "\n";
		std::cout << "Sending block to chain\n";
		json block = curr_block;
		std::cout << block << "\n";
		chain.send(boost::asio::buffer(block.dump()));
		curr_block.timestamp++;
		// Block until we read something
		len = chain.read_some(boost::asio::buffer(buf), err);
	}

	return 0;
}