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

#include "kernel.cuh"

host_t my_host = "127.0.0.1";
port_t my_port = 50002;
name_t my_name = "Ben's GPU";

boost::asio::io_context io_ctxt{};

// Total time mining
// Total time running
// Average number of loops per nonce
// Average time per nonce
// Average time per loop (might be redundant?)
// Total number mined
// Max loops
// Max time

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
	std::array<unsigned int, 32> fake_hash{};
	std::array<unsigned int, 8> fake_state{
		0x00112233,
		0x44556677,
		0x8899AABB,
		0xCCDDEEFF,
		0x00102030,
		0x40506070,
		0x8090A0B0,
		0xC0D0E0F0
	};

	for (auto&& elem : fake_state) {
		std::cout << std::hex << elem << ", ";
	}
	std::cout << "\n";

	for (int j = 0; j < 8; ++j) {
		for (int i = 0; i < 4; ++i) {
			fake_hash[i + j * 4] = (fake_state[j] >> (24 - i * 8)) & 0xFF;
		}
	}

	for (auto&& elem : fake_hash) {
		std::cout << std::hex << std::setw(2) << std::setfill('0') << elem << ", ";
	}
	std::cout << "\n";

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

size_t get_small_stamp() {
	using namespace std::chrono;
	return duration_cast<seconds>(get_now().time_since_epoch()).count();
}

timepoint very_start;
nanoseconds total_time;
nanoseconds max_time;
nanoseconds min_time;
size_t num_blocks;

void listener(tcp::socket& chain, Finder& finder, std::array<char, 64>& buf) {
	boost::asio::async_read(chain, boost::asio::buffer(buf), [&](const boost::system::error_code& err, size_t len) {
		if (len == 0) {
			std::cout << "Empty read\n";
			listener(chain, finder, buf);
			return;
		}
		if (err) {
			std::cerr << err.message() << "\n";
			listener(chain, finder, buf);
			return;
		}

		std::cout << "Read new hash!\n";

		std::cout.write(buf.data(), 64);
		std::cout << "\n";

		string hash{buf.data()};
		hash = hash.substr(0, len);

		std::cout << len << " " << hash << "\n";

		finder.set_last_hash(hash);
		listener(chain, finder, buf);
	});
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
		}
	};

	very_start = get_now();
	min_time = 10000min;

	Finder finder{curr_block};

	std::array<char, 64> chain_buf{};
	listener(chain, finder, chain_buf);

	const auto refresher = []() {
		if (io_ctxt.stopped()) {
			std::cout << "IO Was Stopped!\n";
			io_ctxt.restart();
			std::cout << "IO Restarted!\n";
		}
		io_ctxt.poll();
		std::cout << "IO Polled";
	};

	while (true) {
		for (auto& msg : curr_block.messages) {
			std::random_shuffle(msg.begin(), msg.end());
		}

		curr_block.timestamp = get_small_stamp();
		finder.set_last_hash(last_hash);
		std::cout << "Finding new nonce\n";
		auto start = get_now();
		finder.find_nonce(refresher);
		// find_nonce(last_hash, curr_block);
		++num_blocks;
		auto dur = get_now() - start;
		max_time = std::max(dur, max_time);
		min_time = std::min(dur, min_time);
		total_time += dur;
		auto runtime = get_now() - very_start;
		std::cout << "Average block time: " << duration_cast<milliseconds>(total_time / num_blocks).count()
			<< "ms\nMax block time: " << duration_cast<milliseconds>(max_time).count()
			<< "ms\nMin block time: " << duration_cast<milliseconds>(min_time).count()
			<< "ms\nTotal blocks: " << num_blocks
			<< "\nTotal mining time: " << duration_cast<milliseconds>(total_time).count()
			<< "ms\nTotal running time: " << duration_cast<milliseconds>(runtime).count()
			<< "ms\nEfficiency: " << ((float)total_time.count() / runtime.count()) * 100 << "%\n";
		json block = curr_block;
		std::cout << block << "\n";
		std::cout << "Sending block to chain\n";
		chain.send(boost::asio::buffer(block.dump()));
		last_hash = curr_block.hash;
		// Block until we read something
		std::cout << "Waiting until we get something back\n";
		len = chain.read_some(boost::asio::buffer(buf), err);
		std::cout << len << "\n";
		last_hash = string{buf.data()};
		last_hash = last_hash.substr(0, len);
		std::cout << last_hash << "\n";
	}

	return 0;
}