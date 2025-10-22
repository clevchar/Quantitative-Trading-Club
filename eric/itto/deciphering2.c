#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <mach/mach_time.h>

typedef struct {
    char messageType;
    uint16_t stockLocate;
    uint16_t trackingNumber;
    uint64_t timestamp;
    uint64_t referenceNumber;
    uint32_t executedContracts;
    uint32_t crossNumber;
    uint32_t matchNumber;
} Message;

static inline uint64_t read_be(const uint8_t *b, size_t nbytes) {
    uint64_t v = 0;
#pragma clang loop unroll(full)
    for (size_t i = 0; i < nbytes; ++i)
        v = (v << 8) | b[i];
    return v;
}

static inline uint64_t parse_6byte_be_as_u64(const uint8_t *b) {
    // Safe, cache-friendly, portable
    return ((uint64_t)b[0] << 40) |
           ((uint64_t)b[1] << 32) |
           ((uint64_t)b[2] << 24) |
           ((uint64_t)b[3] << 16) |
           ((uint64_t)b[4] << 8)  |
           ((uint64_t)b[5]);
}

static inline Message parseMessage(const uint8_t *msg, size_t len) {
    Message ee = {0};

    if (len < 11) return ee;

    ee.messageType    = msg[0];
    ee.stockLocate    = (uint16_t)((msg[1] << 8) | msg[2]);
    ee.trackingNumber = (uint16_t)((msg[3] << 8) | msg[4]);
    ee.timestamp      = parse_6byte_be_as_u64(msg + 5);

    if (len >= 19) ee.referenceNumber   = read_be(msg + 11, 8);
    if (len >= 23) ee.executedContracts = __builtin_bswap32(*(const uint32_t*)(msg + 19));
    if (len >= 27) ee.crossNumber       = __builtin_bswap32(*(const uint32_t*)(msg + 23));
    if (len >= 31) ee.matchNumber       = __builtin_bswap32(*(const uint32_t*)(msg + 27));

    return ee;
}

int main(void) {
    const uint8_t messageC[] = {
        0x43,0x00,0x01,0x1F,0x1A,0xD9,0x82,0xB4,0xD4,0x00,0x00,0x00,0x00,
        0xB2,0xD1,0x89,0x14,0x00,0x0F,0x42,0x40,0x00,0x4C,0x4B,0x48,0x4E,
        0x00,0x00,0x44,0xC0,0x00,0x00,0x00,0x01
    };

    const uint8_t messageJ[] = {
        0x4A,0x00,0x00,0x1E,0xD5,0x01,0x12,0x20,0xA2,0x00,0x00,0x00,0x00,
        0xB3,0x28,0xA3,0xE4,0x00,0x00,0x00,0x00,0xB3,0x28,0xA3,0xE8,0x00,
        0x00,0xE4,0x10,0x00,0x66,0x14,0xD0,0x00,0x00,0x00,0x05,0x00,0x68,
        0x62,0xA8,0x00,0x00,0x00,0x05
    };

    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);

    const int iterations = 1000000;
    volatile uint64_t dummy = 0;

    uint64_t start = mach_absolute_time();
    Message ee;
    size_t lenC = sizeof messageC;
    size_t lenJ = sizeof messageJ;

    for (int i = 0; i < iterations; i++) {
        ee = parseMessage(messageC, lenC);
        ee = parseMessage(messageJ, lenJ);
        dummy += ee.trackingNumber;
    }

    uint64_t end = mach_absolute_time();

    double elapsed_ns = (double)(end - start) * tb.numer / tb.denom;
    double per_iter = elapsed_ns / iterations;

    printf("Total: %.3f ns\nAvg/iter: %.3f ns\n", elapsed_ns, per_iter);
    printf("\n--- Last Message ---\n");
    printf("Type: %c\nLocate: %u\nTrack: %u\nTimestamp: %" PRIu64 "\n",
           ee.messageType, ee.stockLocate, ee.trackingNumber, ee.timestamp);
    printf("Ref#: %" PRIu64 "\nExec: %u\nCross: %u\nMatch: %u\n",
           ee.referenceNumber, ee.executedContracts, ee.crossNumber, ee.matchNumber);

    if (dummy == 0) printf("");
    return 0;
}
