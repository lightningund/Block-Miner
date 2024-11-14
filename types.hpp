#pragma once
#include <chrono>
#include <memory>
#include <vector>
#include <boost/asio.hpp>
#include "shared.hpp"

#include "json.hpp"
using json = nlohmann::json;

using std::string;
using boost::asio::ip::udp;
using namespace std::chrono;

using rcv_timeout_option = boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>;

using msg_id_t = string;
using host_t = string;
using port_t = short unsigned int;
using name_t = string;

using stamp_t = long long;

using timepoint = time_point<system_clock>;

extern host_t my_host;
extern port_t my_port;
extern name_t my_name;
extern msg_id_t get_msg_id();
extern bool same_ep(const udp::endpoint& a, const udp::endpoint& b);

struct Gossip {
	host_t host;
	port_t port;
	name_t name;
	msg_id_t id;

	Gossip() : host{my_host}, port{my_port}, name{my_name}, id{get_msg_id()} {}
};

// Contains info about the node that received the gossip
struct GossipReply {
	host_t host;
	port_t port;
	name_t name;
};

struct Peer {
	udp::endpoint endpoint;
	timepoint last_msg;
	size_t local_height;
	string local_hash;

	bool operator==(const Peer& p) const {
		return same_ep(endpoint, p.endpoint);
	}
};

struct Receipt {
	string msg;
	udp::endpoint sender;
	timepoint received;
};

struct Request {
	bool done;
	string msg;
	udp::endpoint target;
	timepoint last_send;
	size_t tries;
	string response_type;
	json response;
};