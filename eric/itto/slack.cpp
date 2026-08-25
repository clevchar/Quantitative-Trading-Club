#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <mach/mach_time.h>
#include <arpa/inet.h> // for ntohl

typedef struct {
    char messageType;           // 1 byte alpha # you can determine this from documentation
    int trackingNumber;         // 2-byte unsigned int # for 2, just link the bytes manually
    uint64_t timestamp;         // 6-byte unsigned int # trick
    uint64_t referenceNumber;   // 8-byte unsigned int # cpu has a tool to link
    int executedContracts;      // 4-byte unsigned int # cpu has a tool to link
    int crossNumber;            // 4-byte unsigned int
    int matchNumber;            // 4-byte unsigned int

// 6 byte trick used on the "ee.timestamp" block below
// we need to swap 6bytes from big endian to littel endian for mac
// read 8 bytes right away (very fast)
// use __builtin_bswap64 to swap all 8 bytes 
// shift right by 16 bits to remove the 2 bytes we didnt need

} Message;

Message parseMessage(const unsigned char *msg) {
    Message ee;

    ee.messageType = msg[0];
    ee.trackingNumber = (msg[1] << 8) | msg[2];

    uint64_t tmp;
    memcpy(&tmp, msg + 3, sizeof(tmp));
    tmp = __builtin_bswap64(tmp);
    ee.timestamp = tmp >> 16;  // remove the two "extra" bytes on the right

    uint64_t tmp1;
    memcpy(&tmp1, msg + 9, sizeof(tmp1));
    ee.referenceNumber = __builtin_bswap64(tmp1);

    uint32_t tmp2;
    memcpy(&tmp2, msg + 17, sizeof(tmp2));
    ee.executedContracts = ntohl(tmp2);

    uint64_t tmp3;
    memcpy(&tmp3, msg + 21, sizeof(tmp3));
    ee.crossNumber = ntohl(tmp3);

    uint64_t tmp4;
    memcpy(&tmp4, msg + 25, sizeof(tmp4));
    ee.matchNumber = ntohl(tmp4);

    return ee;
}

int main() {
    unsigned char message[] = {
        0x43, 0x00, 0x01, 0x1F, 0x1A, 0xD9, 0x82, 0xB4, 0xD4,
        0x00, 0x00, 0x00, 0x00, 0xB2, 0xD1, 0x89, 0x14,
        0x00, 0x0F, 0x42, 0x40, 0x00, 0x4C, 0x4B, 0x48,
        0x4E, 0x00, 0x00, 0x44, 0xC0, 0x00, 0x00, 0x00, 0x01
    };

    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);

    const int iterations = 10000000;
    volatile uint64_t dummy = 0;

    uint64_t start = mach_absolute_time();
    Message ee; // declare once for printing later
    for (int i = 0; i < iterations; i++) {
        ee = parseMessage(message);
        dummy += ee.trackingNumber;
    }
    uint64_t end = mach_absolute_time();

    double elapsed_ns = (double)(end - start) * (double)timebase.numer / (double)timebase.denom;
    double elapsed_per_iter = elapsed_ns / iterations;

    printf("Total elapsed time: %.4f ns\n", elapsed_ns);
    printf("Average per iteration: %.4f ns\n", elapsed_per_iter);

    if (dummy == 0) printf(""); // avoid unused variable warning

    // print the last parsed message
    printf("\n--- Last Parsed Message ---\n");
    printf("Message Type: %c\n", ee.messageType);
    printf("Tracking Number: %d\n", ee.trackingNumber);
    printf("Timestamp: %llu\n", ee.timestamp);
    printf("Reference Number: %llu\n", ee.referenceNumber);
    printf("Executed Contracts: %d\n", ee.executedContracts);
    printf("Cross Number: %d\n", ee.crossNumber);
    printf("Match Number: %d:\n", ee.matchNumber);

    return 0;
}