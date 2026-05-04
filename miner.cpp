#include <iostream>
#include <array>
#include "types.hpp"
#include "kernel.cuh"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Block, minedBy, messages, nonce, height, hash, timestamp)

boost::system::error_code err;
boost::asio::io_context io_ctxt{};
std::array<char, 1024> buf;

timepoint very_start;
nanoseconds total_time;
nanoseconds max_time;
nanoseconds min_time;
size_t num_blocks;

string last_hash;

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

	// Verify that the hash function works correctly
	std::cout << "Reference:  " << test_block.hash << "\n";
	std::cout << "Calculated: " << hash_block("", test_block) << "\n";

	std::cout << "Finding Test Nonce\n";

	Finder f{test_block};
	f.set_last_hash("");
	f.find_nonce();
}

size_t get_small_stamp() {
	return duration_cast<seconds>(get_now().time_since_epoch()).count();
}

void listener(tcp::socket& chain, Finder& finder, std::array<char, 64>& buf) {
	boost::asio::async_read(chain, boost::asio::buffer(buf), [&](const boost::system::error_code& err, size_t len) {
		if (len == 0) {
			std::cout << "Empty read\n";
			listener(chain, finder, buf);
			return;
		}
		if (err) {
			log_err(err.message());
			listener(chain, finder, buf);
			return;
		}

		string hash{buf.data()};
		hash = hash.substr(0, len);
		std::cout << "\rRead new hash! " << hash;

		finder.set_last_hash(hash);
		last_hash = hash;

		listener(chain, finder, buf);
	});
}

string read(tcp::socket& sock) {
	size_t len = sock.read_some(boost::asio::buffer(buf), err);
	std::cout << "Read " << len << " bytes\n";

	string resp{buf.data()};
	return resp.substr(0, len);
}

int main(int argc, char* argv[]) {
	test_hash();

	if (argc < 2) {
		log_err("Please give me a host idk what to do");
		return -1;
	}

	std::srand(std::time(nullptr));

	std::cout << "Connecting to " << argv[1] << "\n";

	tcp::resolver resolver{io_ctxt};
	auto points = resolver.resolve(argv[1], "50001");
	tcp::socket chain{io_ctxt};
	boost::asio::connect(chain, points);

	// size_t len = chain.read_some(boost::asio::buffer(buf), err);
	// std::cout << "Read " << len << " bytes from chain\n";
	// string resp{buf.data()};
	// resp = resp.substr(0, len);
	// std::cout << resp << "\n";
	// size_t idx = std::stoi(resp);

	// string resp = read(chain);

	size_t idx = 0;

	string msg = "Sup hoe";
	chain.send(boost::asio::buffer(msg));

	// std::cout << "Waiting for last hash\n";
	// len = chain.read_some(boost::asio::buffer(buf), err);
	// last_hash = string{buf.data()};
	// last_hash = last_hash.substr(0, len);

	// last_hash = read(chain);

	last_hash = "";

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
			// std::cout << "IO Was Stopped!\n";
			io_ctxt.restart();
			// std::cout << "IO Restarted!\n";
		}
		io_ctxt.poll();
		// std::cout << "IO Polled";
	};

	while (true) {
		// Shuffle the messages so we get different blocks, just for fun
		for (auto& msg : curr_block.messages) {
			std::random_shuffle(msg.begin(), msg.end());
		}

		curr_block.timestamp = get_small_stamp();
		finder.set_last_hash(last_hash);
		std::cout << "Finding new nonce\n";
		auto start = get_now();
		finder.find_nonce(refresher, idx);
		++num_blocks;
		auto dur = get_now() - start;
		max_time = std::max(dur, max_time);
		min_time = std::min(dur, min_time);
		total_time += dur;
		auto runtime = get_now() - very_start;
		std::cout << "\nAverage block time: " << duration_cast<milliseconds>(total_time / num_blocks).count()
			// << "ms\nMax block time: " << to_millis(max_time)
			<< "ms\nMax block time: " << duration_cast<milliseconds>(max_time).count()
			<< "ms\nMin block time: " << duration_cast<milliseconds>(min_time).count()
			<< "ms\nTotal blocks: " << num_blocks
			<< "\nTotal mining time: " << duration_cast<milliseconds>(total_time).count()
			<< "ms\nTotal running time: " << duration_cast<milliseconds>(runtime).count()
			<< "ms\nEfficiency: " << ((float)total_time.count() / runtime.count()) * 100 << "%\n";
		json block = curr_block;
		// std::cout << block << "\n";
		// std::cout << "Sending block to chain\n";
		chain.send(boost::asio::buffer(block.dump()));
		last_hash = curr_block.hash;
		std::cout << "Our hash: " << last_hash << "\n";
		std::cout << "Sent hash: " << block["hash"] << "\n";
		// Block until we read something
		// std::cout << "Waiting until we get something back\n";
		// len = chain.read_some(boost::asio::buffer(buf), err);
		// // std::cout << len << "\n";
		// last_hash = string{buf.data()};
		// last_hash = last_hash.substr(0, len);
		// std::cout << "Read in hash: " << last_hash << "\n";
	}

	return 0;
}
