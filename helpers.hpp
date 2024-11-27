#pragma once
#include "types.hpp"

#define LOG_ERR(msg) std::cerr << (msg) << ": " << errno << "\n"

constexpr auto peers_to_repeat_to = 3;
constexpr auto msg_dead_time = 2s;
constexpr auto peer_dead_time = 1min;
constexpr auto re_gossip_time = 30s;
constexpr auto self_check_time = 1s;
constexpr auto mine_check_time = 5min;
constexpr auto peer_scan_time = 10s;
constexpr auto max_tries = 20;
constexpr auto consensus_time = 5min;

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