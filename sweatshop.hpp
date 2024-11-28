#pragma once
#include <vector>
#include <array>
#include <functional>
#include "types.hpp"

using FoundCallback = std::function<void(std::string)>;

class Sweatshop {
	private:
		// For the miner we will accept next
		tcp::socket* next_miner;
		std::vector<tcp::socket*> miners{};
		std::vector<std::array<char, 1024>> miner_bufs{};

		tcp::acceptor acceptor;

		const FoundCallback found_cb;

		std::string last_hash;
		bool working;

		void check_for_volunteers();
		void listen_for_gold(size_t idx);
		void listen_for_gold();
	public:
		Sweatshop();
		Sweatshop(const FoundCallback cb);
		~Sweatshop();

		void announce_hash(std::string hash);
		void io_update();
};