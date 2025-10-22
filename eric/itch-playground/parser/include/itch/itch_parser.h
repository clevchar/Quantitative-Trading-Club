#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>

namespace itch {

struct AddMessage {
    uint64_t timestamp; // nanoseconds since midnight (spec varies)
    uint64_t order_id;
    char side; // 'B' or 'S'
    uint32_t shares;
    uint32_t price; // price in integer ticks
    char stock[9]; // null-terminated
};

class Parser {
public:
    using AddHandler = std::function<void(const AddMessage&)>;

    Parser() = default;

    // feed a buffer of bytes to the parser; parser will call handlers for messages parsed
    void feed(const uint8_t* buf, size_t len);

    void set_add_handler(AddHandler h) { add_handler_ = std::move(h); }

private:
    AddHandler add_handler_;
    // leftover bytes when a message is split across buffers
    std::vector<uint8_t> leftover_;
};

} // namespace itch
