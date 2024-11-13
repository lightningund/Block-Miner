#include <iostream>

#include "types.hpp"
#include "helpers.hpp"

#include "json.hpp"
using json = nlohmann::json;

// For intellisense
#include <boost/asio.hpp>
using boost::asio::ip::udp;

host_t my_host = "127.0.0.1";
port_t my_port = 50001;
name_t my_name = "Ben's GPU";

boost::asio::io_context io_ctxt{};

#include "kernel.hpp"

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Please Give me a host idk what to do\n";
		return -1;
	}

	udp::endpoint us_ep = udp::endpoint{udp::v4(), my_port};

	udp::socket us_sock = udp::socket{io_ctxt, us_ep};

	std::cout << "Made Socket\n";

	my_host = boost::asio::ip::host_name();

	std::cout << my_host << "\n";

	udp::resolver resolver{io_ctxt};
	udp::endpoint chain = *resolver.resolve({udp::v4(), argv[1], "50000"});

	us_sock.send_to(boost::asio::buffer("yo"), chain);

	return 0;
}