#pragma once
#include "types.hpp"

#define LOG_ERR(msg) std::cerr << (msg) << ": " << errno << "\n"

constexpr auto peers_to_repeat_to = 3;
constexpr auto msg_dead_time = 30s;

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

bool same_ep(const boost::asio::ip::udp::endpoint& a, const boost::asio::ip::udp::endpoint& b) {
	return a.address() == b.address() && a.port() == b.port();
}