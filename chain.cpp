// BREAK IN CASE OF WRONG CHAIN:
// echo '{"type":"CONSENSUS"}' | nc -u 127.0.0.1 8470
// (From the ember server itself)

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
#include "types.hpp"
#include "csha256.hpp"
#include "sweatshop.hpp"

std::vector<std::pair<string, string>> known_hosts{{"silicon.cs.umanitoba.ca", "8999"}, {"eagle.cs.umanitoba.ca", "8999"}, {"grebe.cs.umanitoba.ca", "8999"}, {"hawk.cs.umanitoba.ca", "8999"}};
constexpr auto difficulty = "00000000";

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Block, minedBy, messages, nonce, height, hash, timestamp)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Gossip, host, port, name, id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GossipReply, host, port, name)

host_t my_host = "192.168.102.146";
port_t my_port = 8470;
name_t my_name = "Ben's Computer";

static boost::asio::io_context io_ctxt{};

static string hash_block(string last_hash, Block block) {
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
static void test_hash() {
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

// Runs some simple to checks to see if the block is valid at a glance
static bool quick_check_block(const Block& b) {
	// Not actually real
	if (b.height == -1) return false;
	// Wrong number of zeroes
	if (!b.hash.ends_with(difficulty)) {
		log_err("Not good enough hash");
		return false;
	}
	// Reported hash is the wrong length somehow
	if (b.hash.length() != 64) {
		log_err("Fucked up hash");
		return false;
	}
	// Too many messages
	if (b.messages.size() > 10) {
		log_err("Too many messages");
		return false;
	}
	// No messages
	if (b.messages.size() == 0) {
		log_err("No Messages");
		return false;
	}
	// Any messages that are too long
	for (auto&& msg : b.messages) {
		if (msg.length() > 20) {
			log_err("Message too long");
			return false;
		}
	}
	return true;
}

static inline string block_req_template(size_t idx) {
	return "{\"type\":\"GET_BLOCK\",\"height\":" + std::to_string(idx) + "}";
}

class Chain {
	private:
		std::unordered_set<msg_id_t> sent_gossips{};
		std::vector<Peer> peers{};
		std::vector<Request> reqs{};

		timepoint next_self_check;
		timepoint next_mine_check;
		timepoint next_consensus;
		timepoint next_chain_check;
		timepoint last_gossip;

		udp::resolver udp_res{io_ctxt};
		udp::socket us_sock{io_ctxt, udp::endpoint{udp::v4(), my_port}};

		bool in_consensus = false;
		bool chain_verified = false;

		std::array<char, 1024> recv_buf{};
		Receipt recv_receipt;

		std::vector<Block> chain{};
		bool new_block_made = false;

		std::vector<Peer> agree_peers{};
		std::vector<udp::endpoint> wrong_peers{};

		Sweatshop workers;
		string global_last_hash;

		bool verify_idx(size_t idx) {
			try {
				Block b = chain.at(idx);
				if (!quick_check_block(b)) throw std::runtime_error{"Invalid Block"};
				if (idx > 0) {
					if (chain[idx - 1].height != -1) {
						if (hash_block(chain[idx - 1].hash, b) != b.hash) throw "Reported Hash is Incorrect";
					}
				}
			} catch(const std::exception& err) {
				log_err("Verifying " + std::to_string(idx));
				log_err(err.what());
				return false;
			}

			return true;
		}

		bool add_block(Block b) {
			if (!quick_check_block(b)) return false;
			if (b.height != chain.size()) return false;
			if (chain.size() > 0 && chain_verified && hash_block(chain[chain.size() - 1].hash, b) != b.hash) return false;
			global_last_hash = b.hash;
			chain.push_back(b);
			workers.announce_hash(b.hash);
			return true;
		}

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
				log_err(e.what());
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

			udp::endpoint target = *udp_res.resolve({udp::v4(), incoming.host, std::to_string(incoming.port)});
			GossipReply reply{my_host, my_port, my_name};
			json reply_json = reply;
			reply_json["type"] = "GOSSIP_REPLY";
			send(reply_json, target);
			add_peer(target);

			json goss_json = incoming;
			goss_json["type"] = "GOSSIP";

			std::cout << "Forwarding gossip to ";
			for (int i = 0; i < peers_to_repeat_to; ++i) {
				size_t idx = std::rand() % peers.size();
				std::cout << idx << ", ";
				send(goss_json, peers[idx].endpoint);
			}
			std::cout << "\n";

			last_gossip = get_now();

			sent_gossips.insert(incoming.id);
		}

		void send_stats(const udp::endpoint& target) {
			if (chain.size() > 0) {
				json reply;
				reply["height"] = chain.size();
				reply["hash"] = chain[chain.size() - 1].hash;
				reply["type"] = "STATS_REPLY";
				std::cout << "Stats: " << reply << "\n\tto " << target.address().to_string() << "\n";
				send(reply, target);
			}
		}

		void make_request(udp::endpoint& recip, string msg, string response_type) {
			send(msg, recip);

			reqs.push_back(Request{
				.msg = msg,
				.target = recip,
				.last_send = get_now(),
				.response_type = response_type,
			});
		}

		void make_request(Peer& recip, string msg, string response_type) {
			make_request(recip.endpoint, msg, response_type);
		}

		void get_block(size_t idx, const udp::endpoint& target) {
			try {
				if (chain.size() > 0) {
					Block b = chain.at(idx);

					if (b.height == -1) {
						// We really should have this one
						if (idx < chain.size()) {
							for (auto& peer : agree_peers) {
								make_request(peer, block_req_template(idx), "GET_BLOCK_REPLY");
							}
						}
						throw std::runtime_error{"Sorry, seems we don't have " + std::to_string(idx)};
					}

					json reply = b;
					reply["type"] = "GET_BLOCK_REPLY";
					std::cout << "Block: " << reply << "\n";
					send(reply, target);
				} else {
					throw std::runtime_error{"Empty Chain"};
				}
			} catch (const std::exception& e) {
				log_err(e.what());
				send(json{e.what()}, target);
			}
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

		// Called when we find an erroneous block while verifying
		void LIES() {
			chain_verified = false;
			chain.clear();
			// Say that everyone who suggested this chain was a dirty liar
			for (auto& peer : agree_peers) {
				wrong_peers.push_back(peer.endpoint);
			}
			agree_peers.clear();
			reqs.clear();
			request_stats();
		}

		void verify_chain() {
			chain_verified = true;

			if (chain.size() == 0) return;

			string last_hash = "";

			for (int i = 0; i < chain.size(); ++i) {
				std::cout << i << "\n";
				try {
					Block block = chain.at(i);
					json bl = block;
					std::cout << bl << "\n";
					if (block.height == -1) throw std::runtime_error("Block Missing");

					string hash = hash_block(last_hash, block);
					last_hash = block.hash;
					if (hash != block.hash) throw std::runtime_error("Wrong Hash");
				} catch(const std::exception& e) {
					log_err(e.what());
					log_err("\n\n\nNOOOOOOO!! An error or exception or some kind of unexpected and unhandled circumstance!\n\n\n");
					LIES();
				}
			}

			if (!in_consensus) {
				Block b = chain[chain.size() - 1];
				workers.announce_hash(b.hash);
			}
		}

		void get_blocks(size_t len, std::vector<Peer> agreers) {
			next_chain_check = get_now() + chain_check_time;
			for (long i = len - 1; i >= 0; --i) {
				for (auto& peer : agreers) {
					make_request(peer, block_req_template(i), "GET_BLOCK_REPLY");
				}
			}
		}

		// Go through the chain and make a request for all the blocks we are missing
		void check_chain() {
			if (chain.size() == 0) return;
			timepoint now = get_now();
			if (now < next_chain_check) return;

			next_chain_check = now + chain_check_time;

			for (int i = 0; i < chain.size(); ++i) {
				if (chain[i].height == -1) {
					for (auto& peer : agree_peers) {
						make_request(peer, block_req_template(i), "GET_BLOCK_REPLY");
					}
				}
			}
		}

		// God this function does a lot of loops
		void complete_consensus() {
			std::cout << "Completing Consensus\n";

			size_t longest = 0;

			// Find the longest chain
			for (auto&& req : reqs) {
				std::cout << req.response << "\n";
				for (auto& peer : peers) {
					if (same_ep(peer.endpoint, req.target)) {
						try {
							peer.local_hash = req.response["hash"];
							peer.local_height = req.response["height"];
							if (!peer.local_hash.ends_with(difficulty)) {
								wrong_peers.push_back(peer.endpoint);
								req.response["height"] = 0;
							}
						} catch (const std::exception& err) {
							log_err(err.what());
						}
						break;
					}
				}

				longest = std::max(longest, req.response["height"].template get<size_t>());
			}

			next_consensus = get_now() + consensus_time;
			in_consensus = false;

			reqs.clear();

			// We still have the longest chain, so ignore the plebians
			if (longest <= chain.size()) return;

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

			std::copy_if(peers.begin(), peers.end(), std::back_inserter(agree_peers), [longest, hash](Peer p) {
				return (p.local_height == longest && p.local_hash == hash);
			});

			global_last_hash = hash;
			workers.announce_hash(hash);

			std::cout << "Decided on " << longest << "@" << hash << "\n";

			get_blocks(longest, agree_peers);
		}

		// Create a brand new gossip and send it to the main server
		void make_gossip() {
			std::cout << "Generating Gossip\n";
			json goss = Gossip{};
			goss["type"] = "GOSSIP";
			sent_gossips.insert(goss["id"]);
			for (auto p : peers) {
				std::cout << p.endpoint.address().to_string() << "\n";
				send(goss, p);
			}

			last_gossip = get_now();

			std::cout << "Sent Gossip\n";
		}

		void request_stats() {
			next_consensus = get_now() + consensus_time;
			string msg = "{\"type\": \"STATS\"}";
			in_consensus = true;
			for (auto&& peer : peers) {
				bool wrong = false;
				for (auto& bl_peer : wrong_peers) {
					if (same_ep(bl_peer, peer.endpoint)) {
						wrong = true;
						break;
					}
				}

				if (wrong) continue;

				make_request(peer, msg, "STATS_REPLY");
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
			udp::endpoint silicon = *udp_res.resolve({udp::v4(), known_hosts[0].first, known_hosts[0].second});

			chain = std::vector<Block>(num);

			for (int i = 0; i < chain.size(); ++i) {
				make_request(silicon, block_req_template(i), "GET_BLOCK_REPLY");
			}
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

		void announce_block(Block b) {
			json block = b;
			block["type"] = "ANNOUNCE";

			for (auto& peer : peers) {
				send(block, peer);
			}
		}

		void self_check() {
			timepoint now = get_now();

			if (now < next_self_check) return;
			next_self_check = now + self_check_time;

			if (global_last_hash != "") {
				workers.announce_hash(global_last_hash);
			}

			size_t filled_reqs = count_requests();
			std::cout << "Performing self check, ";
			std::cout << "Performing Consensus: " << (in_consensus ? "Yes, " : "No, ");
			std::cout << "Requests: " << filled_reqs << "/" << reqs.size() << "\n";

			handle_new_block();

			check_chain();

			// Make sure to generate gossip if we haven't sent anything in a while
			if (last_gossip + re_gossip_time < now) {
				make_gossip();
			}

			// Remove peers we haven't heard from
			for (size_t i = known_hosts.size(); i < peers.size(); ++i) {
				if (peers[i].last_msg + peer_dead_time < now) {
					std::erase_if(reqs, [i, this](Request r) {
						return same_ep(peers[i].endpoint, r.target);
					});

					peers.erase(peers.begin() + i);
					--i;
				}
			}

			size_t sent = 0;

			// Re-send requests we haven't received responses to
			for (size_t i = 0; i < reqs.size(); ++i) {
				auto& req = reqs[i];
				if (req.done) continue;
				if (req.last_send + msg_dead_time < now) {
					++req.tries;
					// std::cout << "Resending message " << req.msg << " to " << req.target.address().to_string() << ". Try #" << req.tries << "\n";
					if (req.tries < max_tries) {
						send(req.msg, req.target);
						req.last_send = now;
						++sent;
					}
				}

				if (sent > max_resend) {
					// Move all the requests we went through to the end to make sure we see new ones
					std::rotate(reqs.begin(), reqs.begin() + i, reqs.end());
					break;
				}
			}

			std::erase_if(reqs, [](Request r) { return r.tries >= max_tries; });

			if (in_consensus) {
				if (filled_reqs == reqs.size()) {
					complete_consensus();
				}
			} else {
				if (!chain_verified && reqs.size() == 0) {
					std::cout << "Verifying Chain!\n";
					verify_chain();
				}
			}
		}

		void main_recv() {
			std::fill(recv_buf.begin(), recv_buf.end(), 0);
			us_sock.async_receive_from(boost::asio::buffer(recv_buf), recv_receipt.sender, [this](boost::system::error_code err, size_t len) {
				try {
					if (len == 0) throw std::runtime_error("Empty Read");
					if (err) throw std::runtime_error(err.message());

					// std::cout << "Read something!\n";

					recv_receipt.received = get_now();
					string resp{recv_buf.data()};
					recv_receipt.msg = resp.substr(0, len);

					add_peer(recv_receipt.sender);

					std::cout << recv_receipt.msg.size() << " " << recv_receipt.msg << "\n";

					json incoming = json::parse(recv_receipt.msg);

					Request filled = check_requests(incoming, recv_receipt.sender);
					if (in_consensus) {
						size_t num_filled = count_requests();

						std::cout << num_filled << "/" << reqs.size() << " Requests Filled\n";

						if (filled.done) {
							if (filled.response["height"] > chain.size()) {
								chain = std::vector<Block>(filled.response["height"]);
								global_last_hash = filled.response["hash"];
								workers.announce_hash(global_last_hash);
							}
						}
					} else {
						if (filled.done) { // since check_requests returns an empty request if none were filled, done will be false
							// std::cout << reqs.size() << " Requests Left\n";
							if (filled.response_type == "GET_BLOCK_REPLY" || filled.response_type == "ANNOUNCE") {
								try {
									Block b = filled.response.template get<Block>();
									chain[b.height] = b;
									if (!verify_idx(b.height)) {
										wrong_peers.push_back(filled.target);
										chain[b.height].height = -1;
									}
								} catch(const std::exception& e) {
									log_err(e.what());
								}
							}

							// Clear out completed reqs
							std::erase_if(reqs, [](Request r) { return r.done; });
						}
					}

					if (incoming["type"] == "GOSSIP") {
						process_gossip(incoming.template get<Gossip>());
					} else if (incoming["type"] == "GOSSIP_REPLY") {
						add_peer(incoming["host"], incoming["port"]);
					} else if (incoming["type"] == "STATS") {
						std::cout << "OOOO Sending stats\n";
						send_stats(recv_receipt.sender);
					} else if (incoming["type"] == "ANNOUNCE") {
						add_block(incoming.template get<Block>());
					} else if (incoming["type"] == "GET_BLOCK") {
						get_block(incoming["height"], recv_receipt.sender);
					} else if (incoming["type"] == "CONSENSUS") {
						if (!in_consensus) {
							// It's probably been a while, lets give them another chance
							wrong_peers.clear();
							request_stats();
						}
					}
				} catch(const std::exception& e) {
					log_err(e.what());
				}

				main_recv();
			});
		}

		void main_loop() {
			// std::cout << "New Loop!\n";

			if (io_ctxt.stopped()) {
				// std::cout << "IO Was Stopped!\n";
				io_ctxt.restart();
				// std::cout << "IO Restarted!\n";
			}
			// std::cout << "Trying to poll IO\n";
			io_ctxt.poll();
			// std::cout << "IO Polled\n";

			workers.io_update();

			self_check();
		}

	public:
		Chain(int num_miners) :
			workers{[this](string data) {
				std::cout << data << "\n";
				try {
					json block = json::parse(data);
					block["height"] = chain.size();
					Block new_block = block.template get<Block>();
					bool added = add_block(new_block);

					if (added) announce_block(new_block);
				} catch (const std::exception& err) {
					log_err(err.what());
				}

				if (chain.size() > 0 && chain[chain.size() - 1].hash != "") {
					workers.announce_hash(chain[chain.size() - 1].hash);
				}
			}}
		{
			my_host = boost::asio::ip::host_name();
			std::cout << "Our Address: " << my_host << "\n";
			udp::endpoint public_ep = *udp_res.resolve({udp::v4(), my_host, std::to_string(my_port)});
			my_host = public_ep.address().to_string();
			std::cout << "Our Address: " << my_host << "\n";
			std::cout << "Our Port: " << my_port << "\n";

			for (auto host : known_hosts) {
				udp::endpoint ep = *udp_res.resolve({udp::v4(), host.first, host.second});
				add_peer(ep);
			}

			make_gossip();
			collect_peers();
			request_stats();
			main_recv();

			while (true) {
				try {
					main_loop();
				} catch(const std::exception& e) {
					log_err(e.what());
				}
			}
		}
};

int main(int argc, char* argv[]) {
	int num_miners = 1;
	if (argc > 1) {
		num_miners = std::atoi(argv[1]);
	}

	std::srand(std::time(nullptr));

	Chain chain{num_miners};

	return 0;
}
