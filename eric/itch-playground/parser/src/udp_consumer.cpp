#include "itch/itch_parser.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: consumer_bin <port>\n";
        return 1;
    }
    int port = std::stoi(argv[1]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }

    itch::Parser parser;
    parser.set_add_handler([](const itch::AddMessage& m){
        std::cout << m.timestamp << "," << m.order_id << "," << m.side << "," << m.shares << "," << m.price << "," << m.stock << "\n";
    });

    const size_t BUF = 64 * 1024;
    std::vector<uint8_t> buf(BUF);

    while (true) {
        ssize_t r = recv(sock, buf.data(), BUF, 0);
        if (r < 0) { perror("recv"); break; }
        if (r == 0) continue;
        parser.feed(buf.data(), static_cast<size_t>(r));
    }

    close(sock);
    return 0;
}
