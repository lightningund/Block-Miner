#include <unistd.h>
#include <iostream>
#include <span>
#include <array>
#include <vector>
#include <unordered_set>
#include <chrono>
#include <boost/asio.hpp>

#define LOG_ERR(msg) std::cerr << (msg) << ": " << errno << "\n"

constexpr auto peers_to_repeat_to = 3;

using std::string;
using msg_id_t = string;
using host_t = string;
using port_t = unsigned int;
using name_t = string;

using stamp_t = long long;

stamp_t get_timestamp() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

host_t my_host = "127.0.0.1";
port_t my_port = 50000;
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

string make_gossip_json() {
	string result = "{";
	result += "\"type\": \"GOSSIP\",";
	result += "\"host\": \"" + my_host + "\",";
	result += "\"port\": " + std::to_string(my_port) + ",";
	result += "\"id\": " + std::to_string(get_timestamp()) + ",";
	result += "\"name\": \"" + my_name + "\"}";
	return result;
}

int main() {
	using boost::asio::ip::udp;

	boost::asio::io_context io_ctxt{};

	udp::endpoint us_ep = udp::endpoint{udp::v4(), my_port};
	udp::socket us_sock{io_ctxt, us_ep};

	std::cout << "Made Socket\n";

	my_host = boost::asio::ip::host_name();

	std::cout << "Our Address: " << my_host << "\n";
	std::cout << "Our Port: " << my_port << "\n";

	std::cout << "HMM: " << boost::asio::ip::host_name() << "\n";

	udp::resolver resolver{io_ctxt};
	udp::endpoint silicon = *resolver.resolve({udp::v4(), "silicon.cs.umanitoba.ca", "8999"});

	std::cout << "Made Silicon Endpoint\n";

	string msg = "{\"type\": \"STATS\"}";
	us_sock.send_to(boost::asio::buffer(msg), silicon);

	std::cout << "Sent message\n";

	while (true) {
		std::array<char, 1024> buf;
		boost::system::error_code error;
		size_t len = us_sock.receive(boost::asio::buffer(buf));

		std::cout.write(buf.data(), len);
		std::cout << "\n" << len << "\n";

		if (len < buf.size()) {
			break; // All out of data
		}
	}

	string goss = make_gossip_json();
	std::cout << goss << "\n";
	us_sock.send_to(boost::asio::buffer(goss), silicon);

	std::cout << "Sent Gossip\n";

	while (true) {
		std::array<char, 1024> buf;
		boost::system::error_code error;
		size_t len = us_sock.receive(boost::asio::buffer(buf));

		std::cout.write(buf.data(), len);
		std::cout << "\n" << len << "\n";

		if (len < buf.size()) {
			break; // All out of data
		}
	}

	return 0;
}