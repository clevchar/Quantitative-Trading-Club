#include "itch/itch_parser.h"

#include <cstring>
#include <iostream>

namespace itch {

static uint64_t be_u64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}

static uint32_t be_u32(const uint8_t* p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v = (v << 8) | p[i];
    return v;
}

static uint16_t be_u16(const uint8_t* p) {
    uint16_t v = 0;
    for (int i = 0; i < 2; ++i) v = (v << 8) | p[i];
    return v;
}

static uint64_t be_u48(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 6; ++i) v = (v << 8) | p[i];
    return v;
}

void Parser::feed(const uint8_t* buf, size_t len) {
    // assemble buffer with leftover
    std::vector<uint8_t> data;
    if (!leftover_.empty()) {
        data.reserve(leftover_.size() + len);
        data.insert(data.end(), leftover_.begin(), leftover_.end());
        data.insert(data.end(), buf, buf + len);
        leftover_.clear();
    } else {
        data.assign(buf, buf + len);
    }

    // Scanner mode: try to find candidate 'A' bytes and attempt to parse an Add message
    size_t i = 0;
    const size_t total = data.size();
    // layout after 'A': stockLocate(2)+tracking(2)+timestamp(6) + orderRef(8)+side(1)+shares(4)+stock(8)+price(4)
    const size_t add_payload = 2 + 2 + 6 + 8 + 1 + 4 + 8 + 4;
    while (i + 1 <= total) {
        // find the next 'A' byte
        auto it = std::find(data.begin() + i, data.end(), static_cast<uint8_t>('A'));
        if (it == data.end()) break;
        size_t pos = static_cast<size_t>(it - data.begin());

        // check bounds: need one byte for type + payload
        if (pos + 1 + add_payload > total) {
            // keep leftover from pos to end
            leftover_.assign(data.begin() + pos, data.end());
            return;
        }

        size_t cur = pos + 1; // after type
        AddMessage msg{};
        // header
        uint16_t stock_loc = be_u16(&data[cur]); cur += 2;
        uint16_t track_num = be_u16(&data[cur]); cur += 2;
        uint64_t ts48 = be_u48(&data[cur]); cur += 6;
        // payload
        msg.order_id = be_u64(&data[cur]); cur += 8;
        msg.side = static_cast<char>(data[cur]); cur += 1;
        msg.shares = be_u32(&data[cur]); cur += 4;
        std::memcpy(msg.stock, &data[cur], 8); cur += 8;
        msg.stock[8] = '\0';
        msg.price = be_u32(&data[cur]); cur += 4;
        // store timestamp as 48-bit value in 64-bit field
        msg.timestamp = ts48;

        // Basic sanity checks: printable stock symbol and reasonable shares
        bool stock_printable = true;
        for (int k = 0; k < 8; ++k) {
            unsigned char c = msg.stock[k];
            if (c != ' ' && (c < 32 || c > 126)) { stock_printable = false; break; }
        }

        if (stock_printable && msg.shares > 0 && msg.shares < 100000000) {
            if (add_handler_) add_handler_(msg);
            // advance past this candidate
            i = cur;
        } else {
            // not a valid Add here; continue search after pos
            i = pos + 1;
        }
    }
    // nothing left to keep
}

} // namespace itch
