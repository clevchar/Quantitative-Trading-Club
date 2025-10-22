#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: simulator_bin <decompressed_itch_file> <dest_ip> <dest_port> [--burst] [chunk_size]\n";
        return 1;
    }
    const char* path = argv[1];
    const char* dest_ip = argv[2];
    int dest_port = std::stoi(argv[3]);
    bool burst = false;
    size_t chunk = 1400;
    for (int i=4;i<argc;++i) {
        std::string s = argv[i];
        if (s == "--burst") burst = true;
        else chunk = std::stoul(s);
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) { std::cerr << "Failed to open file\n"; return 1; }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_port);
    if (inet_pton(AF_INET, dest_ip, &addr.sin_addr) != 1) { std::cerr << "Invalid dest ip\n"; return 1; }

    const size_t BUF = chunk;
    std::vector<uint8_t> buf(BUF);
    while (in) {
        in.read(reinterpret_cast<char*>(buf.data()), BUF);
        std::streamsize r = in.gcount();
        if (r <= 0) break;
        ssize_t s = sendto(sock, buf.data(), r, 0, (sockaddr*)&addr, sizeof(addr));
        if (s < 0) { perror("sendto"); break; }
        if (!burst) std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    close(sock);
    return 0;
}
