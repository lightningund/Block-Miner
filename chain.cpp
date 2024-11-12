#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <span>
#include <array>
#include <vector>
#include <unordered_set>
#include <chrono>
#include <boost/asio.hpp>
#include "json.hpp"

using json = nlohmann::json;
using boost::asio::ip::udp;
using std::string;

#define LOG_ERR(msg) std::cerr << (msg) << ": " << errno << "\n"

constexpr auto peers_to_repeat_to = 3;

using msg_id_t = string;
using host_t = string;
using port_t = short unsigned int;
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
	string minedBy;
	std::span<string> messages; // Each message is <=20 characters, max 10 messages
	string nonce; // Must be under 40 characters
	size_t height;
	size_t timestamp;
	string hash;
};

struct Gossip {
	host_t host;
	port_t port;
	name_t name;
	msg_id_t id;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Gossip, host, port, name, id)

// Contains info about the node that received the gossip
struct GossipReply {
	host_t host;
	port_t port;
	name_t name;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GossipReply, host, port, name)

std::unordered_set<msg_id_t> sent_gossips{};
std::vector<udp::endpoint> peers{};

void add_peer(host_t host, port_t port) {
	auto host_addr = boost::asio::ip::address::from_string(host);
	for (auto&& peer : peers) {
		if (peer.address() == host_addr && peer.port() == port) {
			return;
		}
	}

	peers.push_back(udp::endpoint{host_addr, port});
}

void add_peer(udp::endpoint ep) {
	for (auto&& peer : peers) {
		if (peer.address() == ep.address() && peer.port() == ep.port()) {
			return;
		}
	}

	peers.push_back(ep);
}

void process_gossip(const Gossip& incoming, udp::socket& us_sock) {
	if (sent_gossips.contains(incoming.id)) return;

	sent_gossips.insert(incoming.id);
	GossipReply reply{my_host, my_port, my_name};
	json reply_json = reply;
	reply_json["type"] = "GOSSIP_REPLY";
	us_sock.send_to(boost::asio::buffer(reply_json.dump()), udp::endpoint{boost::asio::ip::address::from_string(incoming.host), incoming.port});

	add_peer(incoming.host, incoming.port);

	json goss_json = incoming;
	goss_json["type"] = "GOSSIP";

	for (int i = 0; i < peers_to_repeat_to; ++i) {
		size_t idx = std::rand() % peers.size();
		std::cout << "Forwarding gossip to " << idx << "\n";
		us_sock.send_to(boost::asio::buffer(goss_json.dump()), peers[idx]);
	}
}

Gossip make_gossip() {
	return {
		.host = my_host,
		.port = my_port,
		.name = my_name,
		.id = std::to_string(get_timestamp())
	};
}

int main() {
	std::srand(std::time(nullptr));

	boost::asio::io_context io_ctxt{};

	udp::endpoint us_ep = udp::endpoint{udp::v4(), my_port};
	udp::socket us_sock{io_ctxt, us_ep};

	std::cout << "Made Socket\n";

	my_host = boost::asio::ip::host_name();

	std::cout << "Our Address: " << my_host << "\n";
	std::cout << "Our Port: " << my_port << "\n";

	udp::resolver resolver{io_ctxt};
	udp::endpoint silicon = *resolver.resolve({udp::v4(), "silicon.cs.umanitoba.ca", "8999"});

	std::cout << "Made Silicon Endpoint\n";

	string msg = "{\"type\": \"STATS\"}";
	us_sock.send_to(boost::asio::buffer(msg), silicon);

	std::cout << "Sent message\n";

	while (true) {
		std::array<char, 1024> buf;
		size_t len = us_sock.receive(boost::asio::buffer(buf));

		std::cout.write(buf.data(), len);
		std::cout << "\n" << len << "\n";

		if (len < buf.size()) {
			break; // All out of data
		}
	}

	json goss = make_gossip();
	goss["type"] = "GOSSIP";
	std::cout << goss << "\n";
	us_sock.send_to(boost::asio::buffer(goss.dump()), silicon);
	sent_gossips.insert(goss["id"]);

	std::cout << "Sent Gossip\n";

	// Wait a while to collect a list of peers
	std::cout << "Listening for Peers\n";
	while (true) {
		std::array<char, 1024> buf;
		udp::endpoint sender;
		size_t len = us_sock.receive_from(boost::asio::buffer(buf), sender);

		add_peer(sender);

		std::cout.write(buf.data(), len);
		std::cout << "\n" << len << "\n";

		string resp{buf.data()};
		json incoming = json::parse(resp.substr(0, len));
		if (incoming["type"] == "GOSSIP") {
			process_gossip(incoming.template get<Gossip>(), us_sock);
		} else if (incoming["type"] == "GOSSIP_REPLY") {

		}
	}

	return 0;
}