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

using timepoint = std::chrono::time_point<std::chrono::system_clock>;

timepoint get_now() {
	return std::chrono::system_clock::now();
}

stamp_t get_timestamp() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(get_now().time_since_epoch()).count();
}

msg_id_t get_msg_id() {
	return std::to_string(get_timestamp());
}

bool same_ep(const udp::endpoint& a, const udp::endpoint& b) {
	return a.address() == b.address() && a.port() == b.port();
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

	Gossip() : host{my_host}, port{my_port}, name{my_name}, id{get_msg_id()} {}
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Gossip, host, port, name, id)

// Contains info about the node that received the gossip
struct GossipReply {
	host_t host;
	port_t port;
	name_t name;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GossipReply, host, port, name)

struct Peer {
	udp::endpoint endpoint;
	timepoint last_msg;
	size_t local_height;
	string local_hash;
};

class Request {
	string msg;
	udp::endpoint target;
	timepoint last_send;
};

std::unordered_set<msg_id_t> sent_gossips{};
std::vector<Peer> peers{};

void add_peer(host_t host, port_t port) {
	auto host_addr = boost::asio::ip::address::from_string(host);
	for (auto&& peer : peers) {
		if (peer.endpoint.address() == host_addr && peer.endpoint.port() == port) {
			peer.last_msg = get_now();
			return;
		}
	}

	peers.push_back(Peer{
		.endpoint = udp::endpoint{host_addr, port},
		.last_msg = get_now()
	});
}

void add_peer(udp::endpoint ep) {
	for (auto&& peer : peers) {
		if (peer.endpoint.address() == ep.address() && peer.endpoint.port() == ep.port()) {
			peer.last_msg = get_now();
			return;
		}
	}

	peers.push_back(Peer{
		.endpoint = ep,
		.last_msg = get_now()
	});
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
		us_sock.send_to(boost::asio::buffer(goss_json.dump()), peers[idx].endpoint);
	}
}

// Fills sender with who we received the data from
// Can receive up to 1024 characters at a time
// Adds the sender to the list of peers
// Also prints the message and the length
// Returns the JSON parsed message
json recv(udp::socket& us_sock, udp::endpoint& sender) {
	std::array<char, 1024> buf;
	size_t len = us_sock.receive_from(boost::asio::buffer(buf), sender);

	add_peer(sender);

	string resp{buf.data()};
	resp = resp.substr(0, len);

	std::cout << len << " " << resp << "\n";
	return json::parse(resp);
}

// Receive for when you don't need the endpoint of the sender
// Can receive up to 1024 characters at a time
// Adds the sender to the list of peers
// Also prints the message and the length
// Returns the JSON parsed message
json recv(udp::socket& us_sock) {
	udp::endpoint _sender;
	return recv(us_sock, _sender);
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

	json goss = Gossip{};
	goss["type"] = "GOSSIP";
	std::cout << goss << "\n";
	us_sock.send_to(boost::asio::buffer(goss.dump()), silicon);
	sent_gossips.insert(goss["id"]);

	std::cout << "Sent Gossip\n";

	// Wait a while to collect a list of peers
	std::cout << "Listening for Peers\n";
	auto finish = get_now() + std::chrono::seconds{10};
	while (get_now() < finish) {
		json incoming = recv(us_sock);

		if (incoming["type"] == "GOSSIP") {
			process_gossip(incoming.template get<Gossip>(), us_sock);
		} else if (incoming["type"] == "GOSSIP_REPLY") {
			add_peer(incoming["host"], incoming["port"]);
		}
	}

	std::cout << "Collected peers\n";

	string msg = "{\"type\": \"STATS\"}";
	for (auto&& peer : peers) {
		us_sock.send_to(boost::asio::buffer(msg), peer.endpoint);
	}

	std::cout << "Asked for stats\n";

	while (true) {
		string resp = recv(us_sock);
		json incoming = json::parse(resp);
		if (incoming["type"] == "GOSSIP") {
			process_gossip(incoming.template get<Gossip>(), us_sock);
		} else if (incoming["type"] == "GOSSIP_REPLY") {
			add_peer(incoming["host"], incoming["port"]);
		} else if (incoming["type"] == "STATS") {
			// Return stats
		} else if (incoming["type"] == "STATS_REPLY") {
			// Update our own stats
		}
	}

	return 0;
}