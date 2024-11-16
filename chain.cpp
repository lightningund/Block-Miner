#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <span>
#include <array>
#include <vector>
#include <unordered_set>
#include <map>
#include <chrono>
#include <utility>

#include "csha256.hpp"

#include "types.hpp"
#include "helpers.hpp"

#include "json.hpp"
using json = nlohmann::json;

// For intellisense
#include <boost/asio.hpp>
using boost::asio::ip::udp;
using boost::asio::ip::tcp;

constexpr auto known_host = "192.168.102.146";
// constexpr auto known_host = "silicon.cs.umanitoba.ca";

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Block, minedBy, messages, nonce, height, hash, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Gossip, host, port, name, id)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GossipReply, host, port, name)

host_t my_host = "192.168.102.146";
port_t my_port = 8470;
name_t my_name = "Ben's Computer";

boost::asio::io_context io_ctxt{};

string hash_block(string last_hash, Block block) {
	string input = last_hash;
	input += block.minedBy;

	for (auto&& msg : block.messages) {
		input += msg;
	}

	uint64_t casted_stamp = static_cast<uint64_t>(block.timestamp);
	char* stamp_chars = reinterpret_cast<char*>(&casted_stamp);
	for (int i = 7; i >= 0; --i) {
		input += stamp_chars[i];
	}
	input += block.nonce;

	std::cout << "Hash Input: " << input << "\n";
	std::cout << "Input Length: " << input.size() << "\n";
	string hash = sha256(input);
	std::cout << "Hash: " << hash << "\n";
	std::cout << "Target Hash: " << block.hash << "\n";
	return hash;
}

// Tests the hash on the very first block
void test_hash() {
	Block test_block{
		.minedBy = "Prof!",
		.messages = {"Keep it", "simple.", "Veni", "vidi", "vici"},
		.nonce = "663135608617883",
		.height = 0,
		.timestamp = 1730910874,
		.hash = "75977fa09516d028befa0695e16c93be20271b66630236d38718e35700000000"
	};

	hash_block("", test_block);
}

template <size_t size>
class UDP_Receiver {
	using ERR = boost::system::error_code;
	public:
		UDP_Receiver(udp::socket& sock) : sock{sock} {}

		Receipt recv_from(const seconds& timeout) {
			Receipt rec;
			std::array<char, 1024> buf;
			size_t len = sock.receive_from(boost::asio::buffer(buf), rec.sender);
			if (len == 0) return {};

			rec.received = get_now();

			string resp{buf.data()};
			rec.msg = resp.substr(0, len);

			return rec;
		}
	private:
		udp::socket& sock;
		std::array<char, size> buf;
		Receipt rec;
		bool done = false;

		void start_recv() {
			done = false;
			sock.async_receive_from(boost::asio::buffer(buf), rec.sender, [&](const ERR& err, size_t len) {
				done = true;
				if (!err) {
					std::cout << "Received!\n" << len << "\n";
					rec.received = get_now();
					string resp{buf.data()};
					rec.msg = resp.substr(0, len);
				} else if (err == boost::asio::error::operation_aborted) {
					// Timed out
					std::cerr << "Receive Timed Out\n";
				} else {
					std::cerr << "Error Of Some Kind\n";
					std::cerr << err.message() << "\n";
				}
			});
		}

		void spin(const seconds& timeout) {
			timepoint finish = get_now() + timeout;
			while (get_now() < finish && !done) {
				io_ctxt.run();
			}

			std::cout << "Done Spinning\n";

			if (!done) {
				std::cout << "No receive tho\n";
				sock.cancel();
			}
		}
};

class Chain {
	private:
		std::unordered_set<msg_id_t> sent_gossips{};
		std::vector<Peer> peers{};
		std::vector<Request> reqs{};

		timepoint next_self_check;
		timepoint next_mine_check;
		timepoint last_gossip;

		std::vector<Block> chain{};

		udp::resolver udp_res{io_ctxt};
		udp::socket us_sock{io_ctxt, udp::endpoint{udp::v4(), my_port}};
		tcp::socket miner_sock{io_ctxt};

		bool in_consensus = false;
		bool chain_verified = false;
		bool new_block_made = false;

		bool miner_enable = true;

		std::array<char, 1024> miner_buf{};

		size_t send(json data, const udp::endpoint& targ) {
			return us_sock.send_to(boost::asio::buffer(data.dump()), targ);
		}

		size_t send(string data, const udp::endpoint& targ) {
			return us_sock.send_to(boost::asio::buffer(data), targ);
		}

		size_t send(json data, const Peer& targ) {
			return us_sock.send_to(boost::asio::buffer(data.dump()), targ.endpoint);
		}

		size_t send(string data, const Peer& targ) {
			return us_sock.send_to(boost::asio::buffer(data), targ.endpoint);
		}

		void add_peer(udp::endpoint ep) {
			// Any peer matches, just set their last heard from time to now and return
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
			try {
				udp::endpoint new_ep = *udp_res.resolve({udp::v4(), host, std::to_string(port)});
				add_peer(new_ep);
			} catch (const std::exception& e) {
				std::cerr << e.what() << "\n";
			}
		}

		// Can receive up to 1024 characters at a time
		// Adds the sender to the list of peers
		// Also prints the message and the length
		Receipt recv(udp::socket& us_sock) {
			Receipt rec;
			std::array<char, 1024> buf;
			size_t len = us_sock.receive_from(boost::asio::buffer(buf), rec.sender);
			if (len == 0) return {};

			rec.received = get_now();

			add_peer(rec.sender);

			string resp{buf.data()};
			rec.msg = resp.substr(0, len);

			std::cout << len << " " << resp << "\n";
			return rec;
		}

		/// @throws `boost::system::system_error` if it can't resolve the address
		void process_gossip(const Gossip& incoming) {
			if (sent_gossips.contains(incoming.id)) return;

			last_gossip = get_now();

			udp::endpoint target = *udp_res.resolve({udp::v4(), incoming.host, std::to_string(incoming.port)});
			GossipReply reply{my_host, my_port, my_name};
			json reply_json = reply;
			reply_json["type"] = "GOSSIP_REPLY";
			send(reply_json, target);
			add_peer(target);

			json goss_json = incoming;
			goss_json["type"] = "GOSSIP";

			for (int i = 0; i < peers_to_repeat_to; ++i) {
				size_t idx = std::rand() % peers.size();
				std::cout << "Forwarding gossip to " << idx << "\n";
				send(goss_json, peers[idx].endpoint);
			}

			sent_gossips.insert(incoming.id);
		}

		void send_stats(const udp::endpoint& target) {
			if (chain.size() > 0 && chain.at(chain.size() - 1).height) {
				json reply;
				reply["height"] = chain.size();
				reply["hash"] = chain[chain.size() - 1].hash;
				reply["type"] = "STATS_REPLY";
				std::cout << "Stats: " << reply << "\n\tto " << target.address().to_string() << "\n";
				send(reply, target);
			}
		}

		bool add_block(Block b) {
			if (b.height != chain.size()) return false;
			if (chain_verified && hash_block(chain[chain.size() - 1].hash, b) != b.hash) return false;
			chain.push_back(b);
			return true;
		}

		void get_block(size_t idx, const udp::endpoint& target) {
			try {
				if (chain.size() > 0) {
					if (chain.at(idx).hash != "") {
						json reply = chain[idx];
						reply["type"] = "GET_BLOCK_REPLY";
						std::cout << "Block: " << reply << "\n";
						send(reply, target);
					} else {
						throw std::runtime_error{"Sorry, seems we don't have that one"};
					}
				} else {
					throw std::runtime_error{"Empty Chain"};
				}
			} catch (const std::exception& e) {
				std::cerr << e.what() << "\n";
				json reply = Block{};
				reply["type"] = "GET_BLOCK_REPLY";
				send(reply, target);
			}
		}

		Request make_request(udp::endpoint& recip, string msg, string response_type) {
			send(msg, recip);

			return Request{
				.msg = msg,
				.target = recip,
				.last_send = get_now(),
				.response_type = response_type,
			};
		}

		// Check to see if the new message is the response to any of a list of requests
		// Returns the request that was filled, otherwise `{}`
		Request check_requests(json response, udp::endpoint sender) {
			for (auto& req : reqs) {
				if (req.done) continue;

				if (same_ep(req.target, sender) && response["type"] == req.response_type) {
					req.done = true;
					req.response = response;
					return req;
				}
			}

			return {};
		}

		size_t count_requests() {
			size_t completed = 0;
			for (const auto& req : reqs) {
				if (req.done) ++completed;
			}

			return completed;
		}

		bool verify_chain() {
			chain_verified = true;

			string last_hash = "";

			for (int i = 0; i < chain.size(); ++i) {
				std::cout << i << "\n";
				try {
					if (chain.at(i).messages.size() == 0) {
						std::cerr << "\n\n\nBlock Missing\n\n\n";
						chain.erase(chain.begin() + i, chain.end());
						return true;
					}

					Block block = chain[i];

					string hash = hash_block(last_hash, block);
					last_hash = block.hash;
					if (hash != block.hash) {
						std::cout << "\n\n\nNOOOOOOO!! hash mismatch!!!!\n\n\n";
						chain.erase(chain.begin() + i, chain.end());
						return true;
					}
				} catch(const std::exception& e) {
					std::cerr << e.what() << "\n";
					std::cout << "\n\n\nNOOOOOOO!! An error or exception or some kind of unexpected and unhandled circumstance!\n\n\n";
					chain.erase(chain.begin() + i, chain.end());
					return true;
				}
			}

			return true;
		}

		void get_blocks(size_t len) {
			udp::endpoint silicon = *udp_res.resolve({udp::v4(), known_host, "8999"});

			for (size_t i = 0; i < len; ++i) {
				reqs.push_back(make_request(silicon, "{\"type\":\"GET_BLOCK\",\"height\":" + std::to_string(i) + "}", "GET_BLOCK_REPLY"));
				// for (auto& peer : agreed) {
				// 	reqs.push_back(make_request(us_sock, peer.endpoint, "{\"type\":\"GET_BLOCK\",\"height\":" + std::to_string(i) + "}", "GET_BLOCK_REPLY"));
				// }
			}
		}

		// God this function does a lot of loops
		void complete_consensus() {
			std::cout << "Consensus Complete\n";
			in_consensus = false;

			size_t longest = 0;

			// Find the longest chain
			for (auto&& req : reqs) {
				std::cout << req.response << "\n";
				for (auto& peer : peers) {
					if (same_ep(peer.endpoint, req.target)) {
						peer.local_hash = req.response["hash"];
						peer.local_height = req.response["height"];
						continue;
					}
				}

				longest = std::max(longest, req.response["height"].template get<size_t>());
			}

			reqs.clear();

			// We still have the longest chain, so ignore the plebians
			if (longest < chain.size()) return;

			chain = std::vector<Block>(longest);

			// Find the most commonly believed hash
			std::map<string, size_t> chains;

			// Categorize the chains by their hash
			for (auto&& peer : peers) {
				if (peer.local_height != longest) continue;

				++chains[peer.local_hash];
			}

			size_t votes = 0;
			string hash = "";

			// Find the most agreed upon
			for (const auto& [key, value] : chains) {
				if (value > votes) {
					votes = value;
					hash = key;
				}
			}

			get_blocks(longest);
		}

		// Create a brand new gossip and send it to the main server
		void make_gossip() {
			std::cout << "Generating Gossip\n";
			Peer known = peers[0];
			// udp::endpoint silicon = *udp_res.resolve({udp::v4(), known_host, "8999"});

			std::cout << known.endpoint.address().to_string() << "\n";

			json goss = Gossip{};
			goss["type"] = "GOSSIP";
			send(goss, known);
			sent_gossips.insert(goss["id"]);

			std::cout << "Sent Gossip\n";
		}

		void request_stats() {
			string msg = "{\"type\": \"STATS\"}";
			in_consensus = true;
			for (auto&& peer : peers) {
				reqs.push_back(make_request(peer.endpoint, msg, "STATS_REPLY"));
			}

			std::cout << "Asked for stats\n";
		}

		void collect_peers() {
			// Wait a while to collect a list of peers
			std::cout << "Listening for Peers\n";
			auto finish = get_now() + peer_scan_time;
			while (get_now() < finish) {
				Receipt rec = recv(us_sock);
				json incoming = json::parse(rec.msg);

				if (incoming["type"] == "GOSSIP") {
					process_gossip(incoming.template get<Gossip>());
				} else if (incoming["type"] == "GOSSIP_REPLY") {
					add_peer(incoming["host"], incoming["port"]);
				}
			}

			std::cout << "Collected peers\n";
		}

		// Just requests the first num blocks from the known peer and verifies them
		void demo_get_chain(size_t num) {
			udp::endpoint silicon = *udp_res.resolve({udp::v4(), known_host, "8999"});

			chain = std::vector<Block>(num);

			for (int i = 0; i < chain.size(); ++i) {
				reqs.push_back(make_request(silicon, "{\"type\":\"GET_BLOCK\",\"height\":" + std::to_string(i) + "}", "GET_BLOCK_REPLY"));
			}
		}

		void check_for_miner() {
			if (!miner_enable) return;
			if (!chain_verified) return;
			// if (get_now() < next_mine_check) return;

			std::cout << "Waiting to read from Miner\n";
			miner_sock.async_read_some(boost::asio::buffer(miner_buf), [this](const boost::system::error_code& err, size_t len) {
				std::cout << "Read from miner!\n";
				next_mine_check = get_now() + mine_check_time;
				if (len == 0) {
					std::cerr << "Empty Read\n";
					check_for_miner();
					return;
				}
				if (err) {
					std::cerr << err.message() << "\n";
					check_for_miner();
					return;
				}

				string rec{miner_buf.data()};
				rec = rec.substr(0, len);
				std::cout << len << "\n";
				std::cout << rec << "\n";
				try {
					json block = json::parse(rec);
					block["height"] = chain.size();
					Block new_block = block.template get<Block>();
					bool added = add_block(new_block);

					if (added) new_block_made = true;
				} catch (std::exception& err) {
					std::cerr << err.what() << "\n";
				}

				miner_sock.send(boost::asio::buffer(chain[chain.size() - 1].hash));
				check_for_miner();
			});
		}

		void handle_new_block() {
			if (!new_block_made) return;
			new_block_made = false;

			json block = chain[chain.size() - 1];
			block["type"] = "ANNOUNCE";

			for (auto& peer : peers) {
				send(block, peer);
			}
		}

		void self_check() {
			timepoint now = get_now();

			if (now < next_self_check) return;
			next_self_check = now + self_check_time;

			std::cout << "Performing self check\n";
			std::cout << "Num Requests: " << reqs.size() << "\n";

			handle_new_block();

			// check_for_miner();

			// Make sure to generate gossip if we haven't sent anything in a while
			if (last_gossip + re_gossip_time < now) {
				make_gossip();
			}

			// Remove peers we haven't heard from
			for (size_t i = 0; i < peers.size(); ++i) {
				if (peers[i].last_msg + peer_dead_time < now) {
					std::erase_if(reqs, [i, this](Request r) {
						return same_ep(peers[i].endpoint, r.target);
					});

					peers.erase(peers.begin() + i);
					--i;
				}
			}

			// Re-send requests we haven't received responses to
			for (auto& req : reqs) {
				if (req.done) continue;
				if (req.last_send + msg_dead_time < now) {
					++req.tries;
					std::cout << "Resending message " << req.msg << " to " << req.target.address().to_string() << ". Try #" << req.tries << "\n";
					if (req.tries < max_tries) {
						send(req.msg, req.target);
						req.last_send = now;
					}
				}
			}

			std::erase_if(reqs, [](Request r) { return r.tries >= max_tries; });

			std::cout << "Self Check Complete\n";
		}

		void main_loop() {
			std::cout << "New Loop!\n";

			if (io_ctxt.stopped()) {
				std::cout << "IO Was Stopped!\n";
				io_ctxt.restart();
				std::cout << "IO Restarted!\n";
			}
			std::cout << "Trying to poll IO\n";
			io_ctxt.poll();
			std::cout << "IO Polled\n";

			self_check();

			std::cout << "Trying to read\n";
			UDP_Receiver<1024> recvr{us_sock};
			Receipt rec = recvr.recv_from(5s);
			if (rec.msg.size() == 0) throw std::runtime_error{"Empty Message"};

			add_peer(rec.sender);

			std::cout << rec.msg.size() << " " << rec.msg << "\n";

			json incoming = json::parse(rec.msg);

			Request filled = check_requests(incoming, rec.sender);
			if (in_consensus) {
				size_t num_filled = count_requests();

				std::cout << num_filled << "/" << reqs.size() << " Requests Filled\n";
				if (num_filled == reqs.size()) {
					complete_consensus();
				}
			} else {
				if (filled.done) { // since check_requests returns an empty request if none were filled, done will be false
					std::cout << reqs.size() << " Requests Left\n";
					if (filled.response_type == "GET_BLOCK_REPLY" || filled.response_type == "ANNOUNCE") {
						try {
							chain[filled.response["height"]] = filled.response.template get<Block>();
						} catch(const std::exception& e) {
							std::cerr << e.what() << '\n';
						}
					}

					// Clear out completed reqs
					std::erase_if(reqs, [](Request r) { return r.done; });
				}

				if (!chain_verified && reqs.size() == 0) {
					std::cout << "Verifying Chain!\n";
					verify_chain();
					if (miner_enable) {
						miner_sock.send(boost::asio::buffer(chain[chain.size() - 1].hash));
						next_mine_check = get_now() + mine_check_time;
						check_for_miner();
					}
				}
			}

			if (incoming["type"] == "GOSSIP") {
				process_gossip(incoming.template get<Gossip>());
			} else if (incoming["type"] == "GOSSIP_REPLY") {
				add_peer(incoming["host"], incoming["port"]);
			} else if (incoming["type"] == "STATS") {
				std::cout << "OOOO Sending stats\n";
				send_stats(rec.sender);
			} else if (incoming["type"] == "STATS_REPLY") {
				// Update our own stats
			} else if (incoming["type"] == "ANNOUNCE") {
				add_block(incoming.template get<Block>());
			} else if (incoming["type"] == "GET_BLOCK") {
				get_block(incoming["height"], rec.sender);
			} else if (incoming["type"] == "CONSENSUS") {
				if (in_consensus) return;
				request_stats();
			}
		}

	public:
		Chain(bool miner_enable) : miner_enable{miner_enable} {
			// my_host = boost::asio::ip::host_name();
			// std::cout << "Our Address: " << my_host << "\n";
			// udp::endpoint public_ep = *udp_res.resolve({udp::v4(), my_host, std::to_string(my_port)});
			// my_host = public_ep.address().to_string();
			std::cout << "Our Address: " << my_host << "\n";

			if (miner_enable) {
				tcp::acceptor acceptor{io_ctxt, tcp::endpoint{tcp::v4(), 50001}};
				std::cout << "Waiting to connect to miner\n";
				acceptor.accept(miner_sock);

				string to_miner = "Ayo bitch";
				miner_sock.send(boost::asio::buffer(to_miner));
				std::cout << "Sent to miner\n";
				std::array<char, 1024> m_buf{};
				size_t m_len = miner_sock.receive(boost::asio::buffer(m_buf));
				string m_resp{m_buf.data()};
				m_resp = m_resp.substr(0, m_len);
				std::cout << m_resp << "\n";
			}

			timeval timeout;
			timeout.tv_usec = 0;
			timeout.tv_sec = 2;
			setsockopt(us_sock.native_handle(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
			// setsockopt(miner.native_handle(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

			std::cout << "Our Address: " << my_host << "\n";
			std::cout << "Our Port: " << my_port << "\n";

			udp::endpoint silicon = *udp_res.resolve({udp::v4(), known_host, "8999"});
			add_peer(silicon);
			make_gossip();

			// demo_get_chain(100);

			// collect_peers();
			request_stats();

			while (true) {
				try {
					main_loop();
				} catch(const std::exception& e) {
					std::cerr << e.what() << "\n";
				}
			}
		}
};

int main(int argc, char* argv[]) {
	bool miner_enable = true;
	if (argc > 1) {
		miner_enable = (bool)std::atoi(argv[1]);
	}

	std::srand(std::time(nullptr));

	Chain chain{miner_enable};

	return 0;
}
