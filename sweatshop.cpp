#include "sweatshop.hpp"
#include <iostream>

static boost::asio::io_context io_ctxt{};

void Sweatshop::check_for_volunteers() {
	next_miner = new tcp::socket{io_ctxt};
	std::cout << "Going Diddy mode (Looking for a miner)\n";
	acceptor.async_accept(*next_miner, [this](const boost::system::error_code& err) {
		std::cout << "New miner connection\n";

		try {
			if (err) throw err;
			string to_miner = std::to_string(miners.size());
			next_miner->send(boost::asio::buffer(to_miner));
			std::cout << "Sent to miner\n";
			std::array<char, 1024> m_buf{};
			size_t m_len = next_miner->receive(boost::asio::buffer(m_buf));
			string m_resp{m_buf.data()};
			m_resp = m_resp.substr(0, m_len);
			std::cout << m_resp << "\n";
			miner_bufs.push_back({});
			miners.push_back(next_miner);

			if (working) {
				next_miner->send(boost::asio::buffer(last_hash));
				listen_for_gold(miners.size() - 1);
			}
		} catch (const std::exception& e) {
			LOG_ERROR("Miner Accept");
			LOG_ERROR(e.what());
		}

		next_miner = nullptr;

		check_for_volunteers();
	});
}

void Sweatshop::listen_for_gold(size_t idx) {
	miners[idx]->async_read_some(boost::asio::buffer(miner_bufs[idx]), [this, idx](const boost::system::error_code& err, size_t len) {
		std::cout << "\033[32m\n\nRead from miner!\033[0m\n\n";
		try {
			if (len == 0) throw std::runtime_error{"Empty Read"};
			if (err) throw err;

			string rec{miner_bufs[idx].data()};
			rec = rec.substr(0, len);
			found_cb(rec);
		} catch (std::exception& err) {
			LOG_ERROR("Miner Read");
			LOG_ERROR(err.what());
		}

		listen_for_gold(idx);
	});
}

void Sweatshop::listen_for_gold() {
	for (int i = 0; i < miners.size(); ++i) {
		listen_for_gold(i);
	}
}

Sweatshop::Sweatshop() : Sweatshop{[](std::string _){}} {}

Sweatshop::Sweatshop(const FoundCallback cb) : found_cb{cb}, acceptor{io_ctxt, tcp::endpoint{tcp::v4(), 50001}} {
	check_for_volunteers();
}

Sweatshop::~Sweatshop() {
	for (auto m : miners) {
		delete m;
	}

	if (next_miner != nullptr) {
		delete next_miner;
	}
}

void Sweatshop::announce_hash(std::string hash) {
	last_hash = hash;
	auto buf = boost::asio::buffer(hash);
	for (auto& miner : miners) {
		try {
			miner->send(buf);
		} catch (const std::exception& err) {
			LOG_ERROR(err.what());
		}
	}

	if (!working) {
		working = true;
		listen_for_gold();
	}
}

void Sweatshop::io_update() {
	if (io_ctxt.stopped()) {
		io_ctxt.restart();
	}
	io_ctxt.poll();
}
