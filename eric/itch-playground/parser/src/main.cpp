#include "itch/itch_parser.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <regex>
#include <algorithm>
#include <cctype>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: parser_bin <decompressed_itch_file>\n";
        return 1;
    }
    const char* path = argv[1];

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open file: " << path << "\n";
        return 1;
    }

    // try to infer date from filename (MMDDYYYY at start)
    std::string path_str(path);
    std::string basename;
    auto pos = path_str.find_last_of("/\\");
    if (pos == std::string::npos) basename = path_str; else basename = path_str.substr(pos+1);
    std::string date_prefix;
    std::smatch m;
    if (std::regex_search(basename, m, std::regex("(\\\d{8})"))) {
        std::string s = m.str(1);
        // MMDDYYYY -> YYYY-MM-DDT
        try {
            int mm = std::stoi(s.substr(0,2));
            int dd = std::stoi(s.substr(2,2));
            int yyyy = std::stoi(s.substr(4,4));
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT", yyyy, mm, dd);
            date_prefix = buf;
        } catch(...) { }
    }

    // CSV header
    std::cout << "timestamp,order_ref,side,shares,price,stock\n";

    itch::Parser parser;
    parser.set_add_handler([&date_prefix](const itch::AddMessage& m){
        // timestamp: nanoseconds since midnight -> HH:MM:SS.nnnnnnnnn
        uint64_t ts = m.timestamp;
        uint64_t seconds = ts / 1000000000ULL;
        uint64_t ns_rem = ts % 1000000000ULL;
        uint64_t hh = seconds / 3600;
        uint64_t mm = (seconds % 3600) / 60;
        uint64_t ss = seconds % 60;
        char tsbuf[64];
        std::snprintf(tsbuf, sizeof(tsbuf), "%02llu:%02llu:%02llu.%09llu", (unsigned long long)hh, (unsigned long long)mm, (unsigned long long)ss, (unsigned long long)ns_rem);

        std::string ts_out = tsbuf;
        if (!date_prefix.empty()) ts_out = date_prefix + ts_out;

        // sanitize stock
        std::string stock(m.stock);
        // trim nulls and spaces
        stock.erase(std::find(stock.begin(), stock.end(), '\0'), stock.end());
        while (!stock.empty() && std::isspace((unsigned char)stock.back())) stock.pop_back();
        // uppercase and keep A-Z0-9.-
        std::string out_stock;
        for (char c : stock) {
            char u = std::toupper((unsigned char)c);
            if ((u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '.' || u == '-') out_stock.push_back(u);
        }

        // price: library stores integer ticks; assume 1/10000 scaling
        double price = double(m.price) / 10000.0;

        std::cout << ts_out << "," << m.order_id << "," << m.side << "," << m.shares << "," << price << "," << out_stock << "\n";
    });

    const size_t BUF = 64 * 1024;
    std::vector<uint8_t> buf(BUF);
    while (in) {
        in.read(reinterpret_cast<char*>(buf.data()), BUF);
        std::streamsize r = in.gcount();
        if (r > 0) parser.feed(buf.data(), static_cast<size_t>(r));
    }

    return 0;
}
