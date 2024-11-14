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

#include "csha256.hpp"

#include "types.hpp"
#include "helpers.hpp"

#include "json.hpp"
using json = nlohmann::json;

// For intellisense
#include <boost/asio.hpp>
using boost::asio::ip::udp;
using boost::asio::ip::tcp;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Block, minedBy, messages, nonce, height, hash, timestamp)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Gossip, host, port, name, id)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GossipReply, host, port, name)

host_t my_host = "127.0.0.1";
port_t my_port = 8470;
name_t my_name = "Ben's Computer";

boost::asio::io_context io_ctxt{};

std::unordered_set<msg_id_t> sent_gossips{};
std::vector<Peer> peers{};
std::vector<Request> reqs{};

timepoint next_self_check;
timepoint last_gossip;

std::vector<Block> chain{};

bool in_consensus = false;
bool chain_verified = false;

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

void process_gossip(udp::socket& us_sock, const Gossip& incoming) {
	if (sent_gossips.contains(incoming.id)) return;

	last_gossip = get_now();

	sent_gossips.insert(incoming.id);
	GossipReply reply{my_host, my_port, my_name};
	json reply_json = reply;
	reply_json["type"] = "GOSSIP_REPLY";
	udp::resolver resolver{io_ctxt};
	udp::endpoint target = *resolver.resolve({udp::v4(), incoming.host, std::to_string(incoming.port)});
	us_sock.send_to(boost::asio::buffer(reply_json.dump()), target);

	add_peer(target);

	json goss_json = incoming;
	goss_json["type"] = "GOSSIP";

	for (int i = 0; i < peers_to_repeat_to; ++i) {
		size_t idx = std::rand() % peers.size();
		std::cout << "Forwarding gossip to " << idx << "\n";
		us_sock.send_to(boost::asio::buffer(goss_json.dump()), peers[idx].endpoint);
	}
}

void send_stats(udp::socket& us_sock, const udp::endpoint& target) {
	if (chain.size() > 0 && chain.at(chain.size() - 1).height) {
		json reply;
		reply["height"] = chain.size();
		reply["hash"] = chain[chain.size() - 1].hash;
		reply["type"] = "STATS_REPLY";
		std::cout << "Stats: " << reply << " to " << target.address().to_string() << "\n";
		us_sock.send_to(boost::asio::buffer(reply.dump()), target);
	}
}

void add_block(Block b) {

}

void get_block(udp::socket& us_sock, size_t idx, const udp::endpoint& target) {
	try {
		if (chain.size() > 0 && chain.at(idx).height) {
			json reply = chain[idx];
			reply["type"] = "GET_BLOCK_REPLY";
			std::cout << "Block: " << reply << "\n";
			us_sock.send_to(boost::asio::buffer(reply.dump()), target);
		}
	}
	catch(const std::exception& e) {
		std::cerr << e.what() << '\n';
	}
}

// Can receive up to 1024 characters at a time
// Adds the sender to the list of peers
// Also prints the message and the length
// Returns the JSON parsed message
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

Request make_request(udp::socket& us_sock, udp::endpoint& recip, string msg, string response_type) {
	us_sock.send_to(boost::asio::buffer(msg), recip);

	return Request{
		.msg = msg,
		.target = recip,
		.last_send = get_now(),
		.response_type = response_type,
	};
}

// Check to see if the new message is the response to any of a list of requests
// Returns the number of completed requests in the list
Request check_requests(std::vector<Request>& requests, json response, udp::endpoint sender) {
	for (auto& req : requests) {
		if (req.done) continue;

		if (same_ep(req.target, sender) && response["type"] == req.response_type) {
			req.done = true;
			req.response = response;
			return req;
		}
	}

	return {};
}

size_t count_requests(const std::vector<Request>& requests) {
	size_t completed = 0;
	for (const auto& req : requests) {
		if (req.done) ++completed;
	}

	return completed;
}

bool verify_chain() {
	chain_verified = true;

	string last_hash = "";

	for (auto&& block : chain) {
		string hash = hash_block(last_hash, block);
		last_hash = block.hash;
		if (hash != block.hash) {
			std::cout << "\n\n\nNOOOOOOO\n\n\n";
			return false;
		}
	}

	return true;
}

// God this function does a lot of loops
void complete_consensus(udp::socket& us_sock) {
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

	for (auto& peer : peers) {
		if (peer.local_hash == hash && peer.local_height == longest) {
			for (size_t i = 0; i < longest; ++i) {
				reqs.push_back(make_request(us_sock, peer.endpoint, "{\"type\":\"GET_BLOCK\",\"height\":" + std::to_string(i) + "}", "GET_BLOCK_REPLY"));
			}
		}
	}
}

// Create a brand new gossip and send it to the main server
void make_gossip(udp::socket& us_sock) {
	std::cout << "Generating Gossip\n";
	udp::resolver resolver{io_ctxt};
	udp::endpoint silicon = *resolver.resolve({udp::v4(), "silicon.cs.umanitoba.ca", "8999"});

	json goss = Gossip{};
	goss["type"] = "GOSSIP";
	us_sock.send_to(boost::asio::buffer(goss.dump()), silicon);
	sent_gossips.insert(goss["id"]);
}

void self_check(udp::socket& us_sock) {
	std::cout << "Performing self check\n";

	timepoint now = get_now();

	next_self_check = now + self_check_time;

	// Make sure to generate gossip if we haven't sent anything in a while
	if (last_gossip + re_gossip_time < now) {
		make_gossip(us_sock);
	}

	// Remove peers we haven't heard from
	for (size_t i = 0; i < peers.size(); ++i) {
		if (peers[i].last_msg + peer_dead_time < now) {
			std::erase_if(reqs, [i](Request r) {
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
				us_sock.send_to(boost::asio::buffer(req.msg), req.target);
				req.last_send = now;
			}
		}
	}

	std::erase_if(reqs, [](Request r) { return r.tries >= max_tries; });
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

// Just requests the first 150 blocks from the known peer and verifies them
void demo_get_chain(udp::socket& us_sock) {
	udp::resolver resolver{io_ctxt};
	udp::endpoint silicon = *resolver.resolve({udp::v4(), "silicon.cs.umanitoba.ca", "8999"});

	chain = std::vector<Block>(150);

	for (int i = 0; i < 150; ++i) {
		reqs.push_back(make_request(us_sock, silicon, "{\"type\":\"GET_BLOCK\",\"height\":" + std::to_string(i) + "}", "GET_BLOCK_REPLY"));
	}

	while (true) {
		if (get_now() > next_self_check) {
			self_check(us_sock);
		}

		Receipt rec = recv(us_sock);
		if (rec.msg == "") {
			std::cout << "Timed Out\n";
			continue;
		}

		json incoming = json::parse(rec.msg);

		Request filled = check_requests(reqs, incoming, rec.sender);

		if (filled.done) { // since check_requests returns an empty request if none were filled, done will be false
			std::cout << reqs.size() << " Requests Left\n";
			if (filled.response_type == "GET_BLOCK_REPLY") {
				chain[filled.response["height"]] = filled.response.template get<Block>();
			}

			// Clear out completed reqs
			std::erase_if(reqs, [](Request r) { return r.done; });
		}

		if (!chain_verified && reqs.size() == 0) {
			std::cout << "Verifying Chain!\n";
			verify_chain();
		}
	}
}

int main() {
	std::srand(std::time(nullptr));

	tcp::acceptor acceptor{io_ctxt, tcp::endpoint{tcp::v4(), 50001}};
	tcp::socket miner{io_ctxt};
	std::cout << "Waiting to connect to miner\n";
	acceptor.accept(miner);

	string to_miner = "Ayo bitch";
	miner.send(boost::asio::buffer(to_miner));
	std::cout << "Sent to miner\n";
	std::array<char, 1024> m_buf{};
	size_t m_len = miner.receive(boost::asio::buffer(m_buf));
	string m_resp{m_buf.data()};
	m_resp = m_resp.substr(0, m_len);
	std::cout << m_resp << "\n";

	udp::endpoint us_ep = udp::endpoint{udp::v4(), my_port};

	udp::socket us_sock = udp::socket{io_ctxt, us_ep};
	const int timeout = 2;
	setsockopt(us_sock.native_handle(), SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
	// us_sock.set_option(rcv_timeout_option{200});

	std::cout << "Made Socket\n";

	my_host = boost::asio::ip::host_name();
	std::cout << "Our Address: " << my_host << "\n";
	udp::resolver resolver{io_ctxt};
	udp::endpoint public_ep = *resolver.resolve({udp::v4(), my_host, std::to_string(my_port)});
	my_host = public_ep.address().to_string();

	std::cout << "Our Address: " << my_host << "\n";
	std::cout << "Our Port: " << my_port << "\n";

	make_gossip(us_sock);

	std::cout << "Sent Gossip\n";

	// demo_get_chain(us_sock);

	// Wait a while to collect a list of peers
	std::cout << "Listening for Peers\n";
	auto finish = get_now() + peer_scan_time;
	while (get_now() < finish) {
		Receipt rec = recv(us_sock);
		json incoming = json::parse(rec.msg);

		if (incoming["type"] == "GOSSIP") {
			process_gossip(us_sock, incoming.template get<Gossip>());
		} else if (incoming["type"] == "GOSSIP_REPLY") {
			add_peer(incoming["host"], incoming["port"]);
		}
	}

	std::cout << "Collected peers\n";

	string msg = "{\"type\": \"STATS\"}";
	in_consensus = true;
	for (auto&& peer : peers) {
		reqs.push_back(make_request(us_sock, peer.endpoint, msg, "STATS_REPLY"));
	}

	std::cout << "Asked for stats\n";

	while (true) {
		if (get_now() > next_self_check) {
			self_check(us_sock);
		}

		Receipt rec = recv(us_sock);
		if (rec.msg == "") {
			std::cout << "Timed Out\n";
			continue;
		}

		json incoming;
		try {
			incoming = json::parse(rec.msg);
		} catch(const std::exception& e) {
			std::cerr << e.what() << "\n";
			continue;
		}

		Request filled = check_requests(reqs, incoming, rec.sender);
		if (in_consensus) {
			size_t num_filled = count_requests(reqs);

			std::cout << num_filled << "/" << reqs.size() << " Requests Filled\n";
			if (num_filled == reqs.size()) {
				complete_consensus(us_sock);
			}
		} else {
			if (filled.done) { // since check_requests returns an empty request if none were filled, done will be false
				std::cout << reqs.size() << " Requests Left\n";
				if (filled.response_type == "GET_BLOCK_REPLY") {
					chain[filled.response["height"]] = filled.response.template get<Block>();
				}

				// Clear out completed reqs
				std::erase_if(reqs, [](Request r) { return r.done; });
			}

			if (!chain_verified && reqs.size() == 0) {
				std::cout << "Verifying Chain!\n";
				verify_chain();
			}
		}

		if (incoming["type"] == "GOSSIP") {
			process_gossip(us_sock, incoming.template get<Gossip>());
		} else if (incoming["type"] == "GOSSIP_REPLY") {
			add_peer(incoming["host"], incoming["port"]);
		} else if (incoming["type"] == "STATS") {
			std::cout << "OOOO Sending stats\n";
			send_stats(us_sock, rec.sender);
		} else if (incoming["type"] == "STATS_REPLY") {
			// Update our own stats
		} else if (incoming["type"] == "ANNOUNCE") {
			add_block(incoming.template get<Block>());
		} else if (incoming["type"] == "GET_BLOCK") {
			get_block(us_sock, incoming["height"], rec.sender);
		}
	}

	return 0;
}