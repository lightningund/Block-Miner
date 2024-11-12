#pragma once
#include <chrono>
#include <boost/asio.hpp>

using std::string;
using boost::asio::ip::udp;
using namespace std::chrono;

using msg_id_t = string;
using host_t = string;
using port_t = short unsigned int;
using name_t = string;

using stamp_t = long long;

using timepoint = time_point<system_clock>;

struct Block {
	string minedBy;
	std::span<string> messages; // Each message is <=20 characters, max 10 messages
	string nonce; // Must be under 40 characters
	size_t height;
	size_t timestamp;
	string hash;
};

extern host_t my_host;
extern port_t my_port;
extern name_t my_name;
extern msg_id_t get_msg_id();

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
	string msg;
	udp::endpoint target;
	timepoint last_send;
	string response_type;
};