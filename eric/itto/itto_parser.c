#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <mach/mach_time.h>
#include <arpa/inet.h>

/* Common header present in most ITTO messages */
typedef struct {
    char messageType;        // 1 byte
    uint16_t stockLocate;    // 2 bytes
    uint16_t trackingNumber; // 2 bytes
    uint64_t timestamp;      // 6 bytes
} ITTOHeader;

/* Read big-endian integer */
static inline uint64_t read_be(const uint8_t *b, size_t nbytes) {
    uint64_t v = 0;
    for (size_t i = 0; i < nbytes; ++i) v = (v << 8) | b[i];
    return v;
}

/* Parse 6-byte timestamp efficiently */
static inline uint64_t parse_timestamp(const uint8_t *b) {
    uint64_t tmp;
    memcpy(&tmp, b, 8);
    tmp = __builtin_bswap64(tmp);
    return tmp >> 16;
}

/* Read 2-byte big-endian uint16 */
static inline uint16_t read_u16(const uint8_t *b) {
    return (uint16_t)((b[0] << 8) | b[1]);
}

/* Read 4-byte big-endian uint32 */
static inline uint32_t read_u32(const uint8_t *b) {
    return (uint32_t)((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
}

/* Read 8-byte big-endian uint64 */
static inline uint64_t read_u64(const uint8_t *b) {
    return read_be(b, 8);
}

/* Copy ASCII field and trim trailing spaces */
static void read_ascii(const uint8_t *b, size_t len, char *out, size_t outlen) {
    size_t n = (len < outlen - 1) ? len : outlen - 1;
    memcpy(out, b, n);
    while (n > 0 && out[n-1] == ' ') n--;
    out[n] = '\0';
}

/* Parse common header (first 11 bytes for most messages) */
static ITTOHeader parse_header(const uint8_t *msg) {
    ITTOHeader h;
    h.messageType = (char)msg[0];
    h.stockLocate = read_u16(msg + 1);
    h.trackingNumber = read_u16(msg + 3);
    h.timestamp = parse_timestamp(msg + 5);
    return h;
}

/* [S] System Event (10 bytes) */
static void parse_S(const uint8_t *msg, size_t len) {
    if (len < 10) return;
    ITTOHeader h = parse_header(msg);
    char eventCode = (char)msg[9];
    
    printf("[S] System Event\n");
    printf("  Timestamp: %" PRIu64 "\n", h.timestamp);
    printf("  Event Code: %c\n", eventCode);
}

/* [R] Options Directory (44 bytes) */
static void parse_R(const uint8_t *msg, size_t len) {
    if (len < 44) return;
    ITTOHeader h = parse_header(msg);
    
    uint32_t optionId = read_u32(msg + 11);
    char symbol[9], securitySymbol[9], expirationDate[7], underlyingSymbol[14], source[4];
    read_ascii(msg + 15, 6, symbol, sizeof(symbol));
    read_ascii(msg + 21, 8, underlyingSymbol, sizeof(underlyingSymbol));
    uint32_t strikePrice = read_u32(msg + 29);
    char optionType = (char)msg[33];
    read_ascii(msg + 34, 8, securitySymbol, sizeof(securitySymbol));
    read_ascii(msg + 42, 3, source, sizeof(source));
    
    printf("[R] Options Directory\n");
    printf("  Option ID: %u\n", optionId);
    printf("  Symbol: %s\n", symbol);
    printf("  Underlying: %s\n", underlyingSymbol);
    printf("  Strike: %u\n", strikePrice);
    printf("  Type: %c\n", optionType);
    printf("  Source: %s\n", source);
}

/* [H] Trading Action (14 bytes) */
static void parse_H(const uint8_t *msg, size_t len) {
    if (len < 14) return;
    ITTOHeader h = parse_header(msg);
    uint32_t optionId = read_u32(msg + 11);
    char currentTradingState = (char)msg[13];
    
    printf("[H] Trading Action\n");
    printf("  Option ID: %u\n", optionId);
    printf("  Trading State: %c\n", currentTradingState);
}

/* [O] Option Open (14 bytes) */
static void parse_O(const uint8_t *msg, size_t len) {
    if (len < 14) return;
    ITTOHeader h = parse_header(msg);
    uint32_t optionId = read_u32(msg + 11);
    char openState = (char)msg[13];
    
    printf("[O] Option Open\n");
    printf("  Option ID: %u\n", optionId);
    printf("  Open State: %c\n", openState);
}

/* [a] Add Order (Short) (26 bytes) */
static void parse_a(const uint8_t *msg, size_t len) {
    if (len < 26) return;
    ITTOHeader h = parse_header(msg);
    uint64_t orderRefNum = read_u64(msg + 11);
    char side = (char)msg[19];
    uint16_t size = read_u16(msg + 20);
    uint32_t optionId = read_u32(msg + 22);
    
    printf("[a] Add Order (Short)\n");
    printf("  Order Ref: %" PRIu64 "\n", orderRefNum);
    printf("  Side: %c\n", side);
    printf("  Size: %u\n", size);
    printf("  Option ID: %u\n", optionId);
}

/* [A] Add Order (Long) (30 bytes) */
static void parse_A(const uint8_t *msg, size_t len) {
    if (len < 30) return;
    ITTOHeader h = parse_header(msg);
    uint64_t orderRefNum = read_u64(msg + 11);
    char side = (char)msg[19];
    uint32_t size = read_u32(msg + 20);
    uint32_t optionId = read_u32(msg + 24);
    uint32_t price = read_u32(msg + 26);
    
    printf("[A] Add Order (Long)\n");
    printf("  Order Ref: %" PRIu64 "\n", orderRefNum);
    printf("  Side: %c\n", side);
    printf("  Size: %u\n", size);
    printf("  Option ID: %u\n", optionId);
    printf("  Price: %u\n", price);
}

/* [j] Add Quote (Short) (37 bytes) */
static void parse_j(const uint8_t *msg, size_t len) {
    if (len < 37) return;
    ITTOHeader h = parse_header(msg);
    uint64_t bidRefNum = read_u64(msg + 11);
    uint64_t askRefNum = read_u64(msg + 19);
    uint16_t bidSize = read_u16(msg + 27);
    uint16_t askSize = read_u16(msg + 29);
    uint32_t optionId = read_u32(msg + 31);
    uint16_t bidPrice = read_u16(msg + 35);
    
    printf("[j] Add Quote (Short)\n");
    printf("  Bid Ref: %" PRIu64 "\n", bidRefNum);
    printf("  Ask Ref: %" PRIu64 "\n", askRefNum);
    printf("  Bid Size: %u, Ask Size: %u\n", bidSize, askSize);
    printf("  Option ID: %u\n", optionId);
}

/* [J] Add Quote (Long) (45 bytes) */
static void parse_J(const uint8_t *msg, size_t len) {
    if (len < 45) return;
    ITTOHeader h = parse_header(msg);
    uint64_t bidRefNum = read_u64(msg + 11);
    uint64_t askRefNum = read_u64(msg + 19);
    uint32_t bidSize = read_u32(msg + 27);
    uint32_t askSize = read_u32(msg + 31);
    uint32_t optionId = read_u32(msg + 35);
    uint32_t bidPrice = read_u32(msg + 39);
    uint32_t askPrice = read_u32(msg + 43);
    
    printf("[J] Add Quote (Long)\n");
    printf("  Bid Ref: %" PRIu64 ", Ask Ref: %" PRIu64 "\n", bidRefNum, askRefNum);
    printf("  Bid Size: %u, Ask Size: %u\n", bidSize, askSize);
    printf("  Option ID: %u\n", optionId);
    printf("  Bid Price: %u, Ask Price: %u\n", bidPrice, askPrice);
}

/* [E] Single Side Executed (29 bytes) */
static void parse_E(const uint8_t *msg, size_t len) {
    if (len < 29) return;
    ITTOHeader h = parse_header(msg);
    uint64_t origRefNum = read_u64(msg + 11);
    uint32_t contracts = read_u32(msg + 19);
    uint32_t crossNumber = read_u32(msg + 23);
    uint32_t matchNumber = read_u32(msg + 27);
    
    printf("[E] Single Side Executed\n");
    printf("  Orig Ref: %" PRIu64 "\n", origRefNum);
    printf("  Contracts: %u\n", contracts);
    printf("  Cross: %u, Match: %u\n", crossNumber, matchNumber);
}

/* [C] Single Side Executed With Price (34 bytes) */
static void parse_C(const uint8_t *msg, size_t len) {
    if (len < 34) return;
    ITTOHeader h = parse_header(msg);
    uint64_t origRefNum = read_u64(msg + 11);
    uint32_t contracts = read_u32(msg + 19);
    uint32_t crossNumber = read_u32(msg + 23);
    uint32_t matchNumber = read_u32(msg + 27);
    uint32_t price = read_u32(msg + 31);
    
    printf("[C] Single Side Executed With Price\n");
    printf("  Orig Ref: %" PRIu64 "\n", origRefNum);
    printf("  Contracts: %u\n", contracts);
    printf("  Cross: %u, Match: %u\n", crossNumber, matchNumber);
    printf("  Price: %u\n", price);
}

/* [X] Order Cancel (21 bytes) */
static void parse_X(const uint8_t *msg, size_t len) {
    if (len < 21) return;
    ITTOHeader h = parse_header(msg);
    uint64_t origRefNum = read_u64(msg + 11);
    uint32_t cancelledContracts = read_u32(msg + 19);
    
    printf("[X] Order Cancel\n");
    printf("  Orig Ref: %" PRIu64 "\n", origRefNum);
    printf("  Cancelled: %u\n", cancelledContracts);
}

/* [u] Replace (Short) (29 bytes) */
static void parse_u(const uint8_t *msg, size_t len) {
    if (len < 29) return;
    ITTOHeader h = parse_header(msg);
    uint64_t origRefNum = read_u64(msg + 11);
    uint64_t newRefNum = read_u64(msg + 19);
    uint16_t size = read_u16(msg + 27);
    
    printf("[u] Replace (Short)\n");
    printf("  Orig Ref: %" PRIu64 " -> New Ref: %" PRIu64 "\n", origRefNum, newRefNum);
    printf("  Size: %u\n", size);
}

/* [U] Replace (Long) (33 bytes) */
static void parse_U(const uint8_t *msg, size_t len) {
    if (len < 33) return;
    ITTOHeader h = parse_header(msg);
    uint64_t origRefNum = read_u64(msg + 11);
    uint64_t newRefNum = read_u64(msg + 19);
    uint32_t size = read_u32(msg + 27);
    uint32_t price = read_u32(msg + 31);
    
    printf("[U] Replace (Long)\n");
    printf("  Orig Ref: %" PRIu64 " -> New Ref: %" PRIu64 "\n", origRefNum, newRefNum);
    printf("  Size: %u, Price: %u\n", size, price);
}

/* [D] Single Side Delete (17 bytes) */
static void parse_D(const uint8_t *msg, size_t len) {
    if (len < 17) return;
    ITTOHeader h = parse_header(msg);
    uint64_t origRefNum = read_u64(msg + 11);
    
    printf("[D] Single Side Delete\n");
    printf("  Orig Ref: %" PRIu64 "\n", origRefNum);
}

/* [G] Single Side Update (26 bytes) */
static void parse_G(const uint8_t *msg, size_t len) {
    if (len < 26) return;
    ITTOHeader h = parse_header(msg);
    uint64_t origRefNum = read_u64(msg + 11);
    char side = (char)msg[19];
    uint32_t price = read_u32(msg + 20);
    uint32_t size = read_u32(msg + 24);
    
    printf("[G] Single Side Update\n");
    printf("  Orig Ref: %" PRIu64 "\n", origRefNum);
    printf("  Side: %c, Price: %u, Size: %u\n", side, price, size);
}

/* [k] Quote Replace (Short) (49 bytes) */
static void parse_k(const uint8_t *msg, size_t len) {
    if (len < 49) return;
    ITTOHeader h = parse_header(msg);
    uint64_t origBidRef = read_u64(msg + 11);
    uint64_t origAskRef = read_u64(msg + 19);
    uint64_t newBidRef = read_u64(msg + 27);
    uint64_t newAskRef = read_u64(msg + 35);
    uint16_t bidSize = read_u16(msg + 43);
    uint16_t askSize = read_u16(msg + 45);
    
    printf("[k] Quote Replace (Short)\n");
    printf("  Bid: %" PRIu64 " -> %" PRIu64 "\n", origBidRef, newBidRef);
    printf("  Ask: %" PRIu64 " -> %" PRIu64 "\n", origAskRef, newAskRef);
    printf("  Sizes: Bid %u, Ask %u\n", bidSize, askSize);
}

/* [K] Quote Replace (Long) (57 bytes) */
static void parse_K(const uint8_t *msg, size_t len) {
    if (len < 57) return;
    ITTOHeader h = parse_header(msg);
    uint64_t origBidRef = read_u64(msg + 11);
    uint64_t origAskRef = read_u64(msg + 19);
    uint64_t newBidRef = read_u64(msg + 27);
    uint64_t newAskRef = read_u64(msg + 35);
    uint32_t bidSize = read_u32(msg + 43);
    uint32_t askSize = read_u32(msg + 47);
    uint32_t bidPrice = read_u32(msg + 51);
    uint32_t askPrice = read_u32(msg + 55);
    
    printf("[K] Quote Replace (Long)\n");
    printf("  Bid: %" PRIu64 " -> %" PRIu64 "\n", origBidRef, newBidRef);
    printf("  Ask: %" PRIu64 " -> %" PRIu64 "\n", origAskRef, newAskRef);
    printf("  Bid Size: %u, Price: %u\n", bidSize, bidPrice);
    printf("  Ask Size: %u, Price: %u\n", askSize, askPrice);
}

/* [Y] Quote Delete (25 bytes) */
static void parse_Y(const uint8_t *msg, size_t len) {
    if (len < 25) return;
    ITTOHeader h = parse_header(msg);
    uint64_t bidRefNum = read_u64(msg + 11);
    uint64_t askRefNum = read_u64(msg + 19);
    
    printf("[Y] Quote Delete\n");
    printf("  Bid Ref: %" PRIu64 "\n", bidRefNum);
    printf("  Ask Ref: %" PRIu64 "\n", askRefNum);
}

/* [Q] Cross Trade (30 bytes) */
static void parse_Q(const uint8_t *msg, size_t len) {
    if (len < 30) return;
    ITTOHeader h = parse_header(msg);
    uint32_t optionId = read_u32(msg + 11);
    uint32_t contracts = read_u32(msg + 15);
    uint32_t crossNumber = read_u32(msg + 19);
    uint32_t matchNumber = read_u32(msg + 23);
    char crossType = (char)msg[27];
    uint32_t price = read_u32(msg + 28);
    
    printf("[Q] Cross Trade\n");
    printf("  Option ID: %u\n", optionId);
    printf("  Contracts: %u\n", contracts);
    printf("  Cross: %u, Match: %u\n", crossNumber, matchNumber);
    printf("  Type: %c, Price: %u\n", crossType, price);
}

/* [I] NOII (35 bytes) */
static void parse_I(const uint8_t *msg, size_t len) {
    if (len < 35) return;
    ITTOHeader h = parse_header(msg);
    uint32_t optionId = read_u32(msg + 11);
    char crossType = (char)msg[15];
    uint32_t pairedContracts = read_u32(msg + 16);
    char imbalanceSide = (char)msg[20];
    uint32_t imbalanceQty = read_u32(msg + 21);
    uint32_t bestBidQty = read_u32(msg + 25);
    uint32_t bestAskQty = read_u32(msg + 29);
    
    printf("[I] NOII\n");
    printf("  Option ID: %u\n", optionId);
    printf("  Cross Type: %c\n", crossType);
    printf("  Paired: %u\n", pairedContracts);
    printf("  Imbalance: %c %u\n", imbalanceSide, imbalanceQty);
    printf("  Best Bid/Ask: %u / %u\n", bestBidQty, bestAskQty);
}

/* Dispatcher */
static void parse_message(const uint8_t *msg, size_t len) {
    if (len == 0) return;
    char type = (char)msg[0];
    
    switch (type) {
        case 'S': parse_S(msg, len); break;
        case 'R': parse_R(msg, len); break;
        case 'H': parse_H(msg, len); break;
        case 'O': parse_O(msg, len); break;
        case 'a': parse_a(msg, len); break;
        case 'A': parse_A(msg, len); break;
        case 'j': parse_j(msg, len); break;
        case 'J': parse_J(msg, len); break;
        case 'E': parse_E(msg, len); break;
        case 'C': parse_C(msg, len); break;
        case 'X': parse_X(msg, len); break;
        case 'u': parse_u(msg, len); break;
        case 'U': parse_U(msg, len); break;
        case 'D': parse_D(msg, len); break;
        case 'G': parse_G(msg, len); break;
        case 'k': parse_k(msg, len); break;
        case 'K': parse_K(msg, len); break;
        case 'Y': parse_Y(msg, len); break;
        case 'Q': parse_Q(msg, len); break;
        case 'I': parse_I(msg, len); break;
        default:
            printf("[?] Unknown message type: %c (0x%02X)\n", type, type);
    }
    printf("\n");
}

int main() {
    // All 19 test messages from your dump
    uint8_t msgS[] = {0x53,0x00,0x00,0x07,0x3E,0xE0,0x35,0xAE,0x45,0x4F};
    uint8_t msgR[] = {0x52,0x00,0x00,0x07,0xD7,0x96,0x11,0x5F,0x18,0x00,0x05,0x3B,0xA3,0x45,0x50,0x41,0x4D,0x20,0x20,0x17,0x06,0x10,0x00,0x21,0x91,0xC0,0x43,0x01,0x45,0x50,0x41,0x4D,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x4E,0x59,0x53};
    uint8_t msgH[] = {0x48,0x00,0x00,0x07,0xD7,0x96,0x1B,0xDC,0x7C,0x00,0x05,0x3B,0xA3,0x54};
    uint8_t msgO[] = {0x4F,0x00,0x05,0x1F,0x1A,0xD9,0x82,0xB4,0xD4,0x00,0x03,0xD5,0x59,0x59};
    uint8_t msga[] = {0x61,0x00,0x00,0x13,0xF8,0xF6,0x49,0x74,0x92,0x00,0x00,0x00,0x00,0xB2,0xD0,0x5E,0x08,0x53,0x00,0x02,0x13,0x45,0x00,0x05,0x00,0x08};
    uint8_t msgA[] = {0x41,0x00,0x00,0x1B,0xBB,0xD2,0x33,0x22,0xBD,0x00,0x00,0x00,0x00,0xB2,0xD1,0x42,0xF0,0x53,0x00,0x00,0x0D,0x51,0x00,0x7A,0x25,0x88,0x00,0x00,0x00,0x01};
    uint8_t msgj[] = {0x6A,0x00,0x00,0x1E,0xD4,0xF5,0x7D,0xBD,0xA2,0x00,0x00,0x00,0x00,0xB3,0x28,0x53,0x68,0x00,0x00,0x00,0x00,0xB3,0x28,0x53,0x6C,0x00,0x01,0xE3,0xC1,0x00,0x78,0x00,0x01,0x02,0x6C,0x00,0x01};
    uint8_t msgJ[] = {0x4A,0x00,0x00,0x1E,0xD5,0x01,0x12,0x20,0xA2,0x00,0x00,0x00,0x00,0xB3,0x28,0xA3,0xE4,0x00,0x00,0x00,0x00,0xB3,0x28,0xA3,0xE8,0x00,0x00,0xE4,0x10,0x00,0x66,0x14,0xD0,0x00,0x00,0x00,0x05,0x00,0x68,0x62,0xA8,0x00,0x00,0x00,0x05};
    uint8_t msgE[] = {0x45,0x00,0x01,0x1F,0x1A,0xE4,0x52,0x30,0x83,0x00,0x00,0x00,0x00,0xB3,0xA0,0x82,0x90,0x00,0x00,0x00,0x01,0x00,0x0F,0x42,0xC8,0x00,0x4C,0x4D,0x08};
    uint8_t msgC[] = {0x43,0x00,0x01,0x1F,0x1A,0xD9,0x82,0xB4,0xD4,0x00,0x00,0x00,0x00,0xB2,0xD1,0x89,0x14,0x00,0x0F,0x42,0x40,0x00,0x4C,0x4B,0x48,0x4E,0x00,0x00,0x44,0xC0,0x00,0x00,0x00,0x01};
    uint8_t msgX[] = {0x58,0x00,0x01,0x1F,0x1C,0x04,0x0B,0x45,0x1C,0x00,0x00,0x00,0x00,0xB3,0x7B,0x95,0xDC,0x00,0x00,0x00,0x03};
    uint8_t msgu[] = {0x75,0x00,0x00,0x1D,0x9D,0x32,0x58,0xC7,0x32,0x00,0x00,0x00,0x00,0xB3,0x05,0x9C,0x0C,0x00,0x00,0x00,0x00,0xB3,0x05,0xB7,0xE0,0x00,0x19,0x00,0x0A};
    uint8_t msgU[] = {0x55,0x00,0x00,0x1E,0xD5,0x06,0x50,0xB1,0xF6,0x00,0x00,0x00,0x00,0xB3,0x28,0xB0,0x14,0x00,0x00,0x00,0x00,0xB3,0x28,0xD0,0xD0,0x00,0x64,0xDE,0x44,0x00,0x00,0x00,0x04};
    uint8_t msgD[] = {0x44,0x00,0x00,0x18,0xEB,0xCA,0xB3,0x7B,0x80,0x00,0x00,0x00,0x00,0xB2,0xD0,0x6C,0xE8};
    uint8_t msgG[] = {0x47,0x00,0x00,0x1E,0xD5,0x62,0x15,0x33,0xF8,0x00,0x00,0x00,0x00,0xB3,0x28,0x80,0x98,0x55,0x00,0x0B,0xEA,0xC8,0x00,0x00,0x00,0x01};
    uint8_t msgk[] = {0x6B,0x00,0x00,0x1E,0xD5,0x00,0xAC,0x76,0xEF,0x00,0x00,0x00,0x00,0xB3,0x28,0x55,0x0C,0x00,0x00,0x00,0x00,0xB3,0x28,0x8E,0xB0,0x00,0x00,0x00,0x00,0xB3,0x28,0x55,0x10,0x00,0x00,0x00,0x00,0xB3,0x28,0x8E,0xB4,0x00,0x00,0x00,0x00,0x01,0xF4,0x00,0x01};
    uint8_t msgK[] = {0x4B,0x00,0x00,0x1E,0xD5,0x62,0x3E,0x27,0x8C,0x00,0x00,0x00,0x00,0xB3,0x28,0xA5,0x24,0x00,0x00,0x00,0x00,0xB3,0x29,0xD9,0xA4,0x00,0x00,0x00,0x00,0xB3,0x28,0xA5,0x28,0x00,0x00,0x00,0x00,0xB3,0x29,0xD9,0xA8,0x00,0x7E,0xB1,0x98,0x00,0x00,0x00,0x05,0x00,0x81,0x61,0x18,0x00,0x00,0x00,0x05};
    uint8_t msgY[] = {0x59,0x00,0x00,0x1E,0xD4,0xF9,0x30,0x08,0x03,0x00,0x00,0x00,0x00,0xB3,0x28,0x55,0x50,0x00,0x00,0x00,0x00,0xB3,0x28,0x55,0x54};
    uint8_t msgQ[] = {0x51,0x00,0x05,0x1F,0x1A,0xD9,0x82,0xB4,0xD4,0x00,0x03,0xD5,0x59,0x00,0x0F,0x42,0x40,0x00,0x4C,0x4B,0x58,0x4F,0x00,0x00,0x44,0xC0,0x00,0x00,0x00,0x02};
    uint8_t msgI[] = {0x49,0x00,0x00,0x1E,0xD4,0xF9,0x6C,0x10,0x1C,0x00,0x0F,0x42,0x44,0x4F,0x00,0x00,0x00,0x01,0x42,0x00,0x00,0x06,0xB7,0x00,0x00,0x12,0xC0,0x00,0x00,0x00,0x00,0x20,0x20,0x20,0x20};

    printf("=== Parsing all 19 ITTO message types ===\n\n");
    
    parse_message(msgS, sizeof(msgS));
    parse_message(msgR, sizeof(msgR));
    parse_message(msgH, sizeof(msgH));
    parse_message(msgO, sizeof(msgO));
    parse_message(msga, sizeof(msga));
    parse_message(msgA, sizeof(msgA));
    parse_message(msgj, sizeof(msgj));
    parse_message(msgJ, sizeof(msgJ));
    // parse_message(msgE, sizeof(msgE));
    parse_message(msgC, sizeof(msgC));
    parse_message(msgX, sizeof(msgX));
    parse_message(msgu, sizeof(msgu));
    parse_message(msgU, sizeof(msgU));
    parse_message(msgD, sizeof(msgD));
    parse_message(msgG, sizeof(msgG));
    parse_message(msgk, sizeof(msgk));
    parse_message(msgK, sizeof(msgK));
    parse_message(msgY, sizeof(msgY));
    parse_message(msgQ, sizeof(msgQ));
    parse_message(msgI, sizeof(msgI));

    printf("=== Performance test ===\n");
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    
    const int iterations = 1000000;
    uint64_t start = mach_absolute_time();
    for (int i = 0; i < iterations; i++) {
        ITTOHeader h = parse_header(msgJ);
        (void)h; // suppress unused warning
    }
    uint64_t end = mach_absolute_time();
    
    double elapsed_ns = (double)(end - start) * timebase.numer / timebase.denom;
    printf("Header parse: %.2f ns per message (1M iterations)\n", elapsed_ns / iterations);

    return 0;
}
