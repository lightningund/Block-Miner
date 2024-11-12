#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <span>
#include <array>
#include <vector>
#include <unordered_set>
#include <chrono>

#include "types.hpp"
#include "helpers.hpp"
#include "json.hpp"

using json = nlohmann::json;
using boost::asio::ip::udp;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Gossip, host, port, name, id)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GossipReply, host, port, name)

host_t my_host = "127.0.0.1";
port_t my_port = 50000;
name_t my_name = "Ben's Computer";

std::unordered_set<msg_id_t> sent_gossips{};
std::vector<Peer> peers{};

void add_peer(udp::endpoint ep) {
	for (auto&& peer : peers) {
		if (same_ep(peer.endpoint, ep)) {
			peer.last_msg = get_now();
			return;
		}
	}

	peers.push_back(Peer{
		.endpoint = ep,
		.last_msg = get_now()
	});
}

void add_peer(host_t host, port_t port) {
	auto host_addr = boost::asio::ip::address::from_string(host);
	udp::endpoint new_ep{host_addr, port};
	add_peer(new_ep);
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

Request make_request(udp::socket& us_sock, udp::endpoint recip, string msg) {
	us_sock.send_to(boost::asio::buffer(msg), recip);

	return Request{
		.msg = msg,
		.target = recip,
		.last_send = get_now()
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