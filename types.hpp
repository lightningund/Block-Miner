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
using boost::asio::ip::tcp;
using namespace std::chrono;

using rcv_timeout_option = boost::asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>;

using msg_id_t = string;
using host_t = string;
using port_t = short unsigned int;
using name_t = string;

using stamp_t = long long;

using timepoint = time_point<system_clock>;

constexpr auto peers_to_repeat_to = 3;
constexpr auto msg_dead_time = 2s;
constexpr auto peer_dead_time = 1min;
constexpr auto re_gossip_time = 30s;
constexpr auto self_check_time = 1s;
constexpr auto mine_check_time = 5min;
constexpr auto peer_scan_time = 10s;
constexpr auto max_tries = 50;
constexpr auto consensus_time = 5min;
constexpr auto max_resend = 100; // Max number of messages to re-send at once
constexpr auto chain_check_time = 30min;

static inline timepoint get_now() {
	return std::chrono::system_clock::now();
}

static inline stamp_t get_timestamp() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(get_now().time_since_epoch()).count();
}

static inline msg_id_t get_msg_id() {
	return std::to_string(get_timestamp());
}

static inline bool same_ep(const udp::endpoint& a, const udp::endpoint& b) {
	return a.address() == b.address() && a.port() == b.port();
}

extern host_t my_host;
extern port_t my_port;
extern name_t my_name;

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