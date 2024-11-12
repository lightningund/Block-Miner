#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <iostream>
#include <span>
#include <array>
#include <vector>
#include <unordered_set>
#include <boost/asio.hpp>

#define LOG_ERR(msg) std::cerr << (msg) << ": " << errno << "\n"

constexpr auto peers_to_repeat_to = 3;

using string = std::string;
using msg_id_t = string;
using host_t = string;
using port_t = unsigned int;
using name_t = string;

host_t my_host = "127.0.0.1";
port_t my_port = 9000;
name_t my_name = "Ben's Computer";

struct Block {
	string miner;
	std::span<string> messages; // Each message is <=20 characters, max 10 messages
	string nonce; // Must be under 40 characters
	// Hash (difficulty 8?) TODO
	// Height? TODO
};

struct Gossip {
	host_t host;
	port_t port;
	name_t name;
	msg_id_t id;
};

// Contains info about the node that received the gossip
struct GossipReply {
	host_t host;
	port_t port;
	name_t name;
};

std::unordered_set<msg_id_t> sent_gossips{};

void process_gossip(const Gossip& incoming) {
	if (sent_gossips.contains(incoming.id)) return;

	sent_gossips.insert(incoming.id);
	GossipReply reply{my_host, my_port, my_name};
	// Send reply to incoming.host @ incoming.port
	// Send incoming to 3 other peers
}

int main() {
	using boost::asio::ip::tcp;

	boost::asio::io_context io_ctxt{};

	tcp::resolver resolver{io_ctxt};

	auto endpoints = resolver.resolve("silicon.cs.umanitoba.ca", "silicon");

	tcp::socket socket{io_ctxt};
	boost::asio::connect(socket, endpoints);

	while (true) {
		std::array<char, 1024> buf;
		boost::system::error_code error;
		size_t len = socket.read_some(boost::asio::buffer(buf), error);

		if (error == boost::asio::error::eof) {
			break; // Server Ended Connection
		} else if (error) {
			throw boost::system::system_error(error); // Real error
		}

		std::cout.write(buf.data(), len);
	}

	// int server_socket = socket(AF_INET, SOCK_DGRAM, 0);

	// if (server_socket < 0) {
	// 	LOG_ERR("chat server socket initialization error");
	// 	return -1;
	// }

	// std::cout << "Socket Created\n";

	// sockaddr_in sock_addr{
	// 	AF_INET, htons(0), inet_addr("")
	// };

	// if (bind(
	// 	server_socket,
	// 	reinterpret_cast<sockaddr*>(&sock_addr),
	// 	sizeof(sock_addr)
	// ) < 0) {
	// 	LOG_ERR("bind error");
	// 	close(server_socket);
	// 	return -1;
	// }

	// std::cout << "Socket Bound\n";

	// timeval timeout;
	// timeout.tv_sec = 10;
	// timeout.tv_usec = 0;

	// if (setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
	// 	LOG_ERR("error setting socket timeout");
	// 	return -1;
	// }

	// std::cout << "Timeout Set\n";

	// sockaddr_in sock_rep{};
	// socklen_t sock_size = sizeof(sock_rep);

	// if (getsockname(server_socket, reinterpret_cast<sockaddr*>(&sock_rep), &sock_size) < 0) {
	// 	LOG_ERR("Error Getting Socket Name");
	// 	return -1;
	// }

	// std::string my_ip(20, '\0');
	// inet_ntop(AF_INET, &sock_rep.sin_addr, my_ip.data(), 20);

	// std::cout << my_ip << "\n" << ntohs(sock_rep.sin_port) << "\n";

	// sockaddr_in silicon_addr{
	// 	AF_INET, htons(8999), inet_addr("silicon.cs.umanitoba.ca")
	// };

	// std::string msg = "{\"type\": \"STATS\"}";
	// if (sendto(server_socket, msg.c_str(), msg.size(), 0, reinterpret_cast<sockaddr*>(&silicon_addr), sizeof(silicon_addr)) < 0) {
	// 	LOG_ERR("Failed to send");
	// 	return -1;
	// }

	// std::cout << msg << " Sent\n";

	// std::string resp(1024, '\0');
	// int recv_len = recv(server_socket, resp.data(), resp.size(), 0);
	// if (recv_len < 0) {
	// 	LOG_ERR("recv error");
	// 	return -1;
	// }

	// std::cout << "Response Received\n";

	// resp = resp.substr(0, recv_len);
	// std::cout << resp << "\n";

	// sockaddr_in server_addr{
	// 	AF_INET, htons(8999), inet_addr("silicon.cs.umanitoba.ca")
	// };

	// // Send connection request to server:
	// if (connect(server_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
	// 	LOG_ERR("connect error");
	// 	return -1;
	// }

	// std::cout << "Connected with server successfully\n";
	// std::cout << "Gossiping\n";
	// // Send the message to server:
	// std::string req_str = "GOSSIP";

	// if (send(server_socket, req_str.c_str(), req_str.size(), 0) < 0) {
	// 	LOG_ERR("Unable to send message\n");
	// 	return -1;
	// }

	// std::cout << "Request sent\n";
	// std::string resp(1000, '\0');

	// // Receive the server's response:
	// int recv_len = recv(server_socket, resp.data(), resp.size(), 0);
	// if (recv_len < 0) {
	// 	LOG_ERR("Error while receiving server's msg\n");
	// 	return -1;
	// }

	// resp = resp.substr(0, recv_len);

	// std::cout << "Messages Received\n" << resp << "\n";

	return 0;
}