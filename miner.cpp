#include <iostream>
#include <array>

#include "types.hpp"
#include "helpers.hpp"

#include "json.hpp"
using json = nlohmann::json;

// For intellisense
#include <boost/asio.hpp>
using boost::asio::ip::tcp;

host_t my_host = "127.0.0.1";
port_t my_port = 50002;
name_t my_name = "Ben's GPU";

boost::asio::io_context io_ctxt{};

#include "kernel.cuh"

int main(int argc, char* argv[]) {
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

	return 0;
}