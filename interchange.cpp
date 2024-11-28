#include <iostream>
#include <array>

#include "shared.hpp"

// For intellisense
#include <boost/asio.hpp>
using boost::asio::ip::tcp;

boost::asio::io_context io_ctxt{};

int main(int argc, char* argv[]) {
	if (argc < 2) {
		LOG_ERROR("Please Give me a host idk what to do");
		return -1;
	}
	if (argc < 3) {
		LOG_ERROR("Please Give me a miner host idk what to do");
		return -1;
	}
	if (argc < 4) {
		LOG_ERROR("Please Give me a miner port idk what to do");
		return -1;
	}

	tcp::resolver resolver{io_ctxt};
	auto points = resolver.resolve(argv[1], "50001");
	tcp::socket chain{io_ctxt};
	boost::asio::connect(chain, points);
	std::array<char, 1024> chain_buf;

	auto points2 = resolver.resolve(argv[2], argv[3]);
	tcp::socket miner{io_ctxt};
	boost::asio::connect(chain, points2);
	std::array<char, 1024> miner_buf;

	auto chain_listen = [&chain, &miner, &chain_buf]() {
		boost::asio::async_read(chain, boost::asio::buffer(chain_buf), [&](const boost::system::error_code& err, size_t len) {
			try {
				if (len == 0) throw std::runtime_error{"Empty Read"};
				if (err) throw err;

				string msg{chain_buf.data()};
				msg = msg.substr(0, len);

				std::cout << "Sending to miner: " << msg << "\n";

				miner.send(boost::asio::buffer(msg));
			} catch (const std::exception& e) {
				LOG_ERROR("Chain Read");
				LOG_ERROR(e.what());
			}
			chain_listen();
		});
	};

	auto miner_listen = [&chain, &miner, &miner_buf]() {
		boost::asio::async_read(miner, boost::asio::buffer(miner_buf), [&](const boost::system::error_code& err, size_t len) {
			try {
				if (len == 0) throw std::runtime_error{"Empty Read"};
				if (err) throw err;

				string msg{miner_buf.data()};
				msg = msg.substr(0, len);

				std::cout << "Sending to chain: " << msg << "\n";

				chain.send(boost::asio::buffer(msg));
			} catch (const std::exception& e) {
				LOG_ERROR("Miner Read");
				LOG_ERROR(e.what());
			}
			miner_listen();
		});
	};

	chain_listen();
	miner_listen();

	while (true) {
		if (io_ctxt.stopped()) {
			io_ctxt.restart();
		}
		io_ctxt.poll();
	}

	return 0;
}