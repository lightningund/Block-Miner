#include <unistd.h>
#include <iostream>
#include <span>
#include <array>
#include <vector>
#include <unordered_set>
#include <boost/asio.hpp>

#define LOG_ERR(msg) std::cerr << (msg) << ": " << errno << "\n"

constexpr auto peers_to_repeat_to = 3;

using string = std::string;
using msg_id_t = string;
using host_t = string;
using port_t = unsigned int;
using name_t = string;

host_t my_host = "127.0.0.1";
port_t my_port = 9000;
name_t my_name = "Ben's Computer";

struct Block {
	string miner;
	std::span<string> messages; // Each message is <=20 characters, max 10 messages
	string nonce; // Must be under 40 characters
	// Hash (difficulty 8?) TODO
	// Height? TODO
};

struct Gossip {
	host_t host;
	port_t port;
	name_t name;
	msg_id_t id;
};

// Contains info about the node that received the gossip
struct GossipReply {
	host_t host;
	port_t port;
	name_t name;
};

std::unordered_set<msg_id_t> sent_gossips{};

void process_gossip(const Gossip& incoming) {
	if (sent_gossips.contains(incoming.id)) return;

	sent_gossips.insert(incoming.id);
	GossipReply reply{my_host, my_port, my_name};
	// Send reply to incoming.host @ incoming.port
	// Send incoming to 3 other peers
}

int main() {
	using boost::asio::ip::udp;

	boost::asio::io_context io_ctxt{};

	udp::socket socket{io_ctxt, udp::endpoint{udp::v4(), 0}};

	std::cout << "Made Socket\n";

	udp::resolver resolver{io_ctxt};
	udp::endpoint silicon = *resolver.resolve({udp::v4(), "silicon.cs.umanitoba.ca", "8999"});

	std::cout << "Made Endpoint\n";

	std::string msg = "{\"type\": \"STATS\"}";
	socket.send_to(boost::asio::buffer(msg), silicon);

	std::cout << "Sent message\n";

	while (true) {
		std::array<char, 1024> buf;
		boost::system::error_code error;
		size_t len = socket.receive(boost::asio::buffer(buf));

		std::cout.write(buf.data(), len);
		std::cout << "\n" << len << "\n";

		if (len < buf.size()) {
			break; // All out of data
		}
	}

	return 0;
}