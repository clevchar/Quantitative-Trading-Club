#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <mach/mach_time.h>
#include <arpa/inet.h> // for ntohl

typedef struct {
	char messageType;           // 1 byte
	uint16_t stockLocate;       // 2 bytes
	uint16_t trackingNumber;    // 2 bytes
	uint64_t timestamp;         // 6 bytes packed into 8

	/* example fields for executed/cross messages */
	uint64_t referenceNumber;   // 8 bytes
	uint32_t executedContracts; // 4 bytes
	uint32_t crossNumber;       // 4 bytes
	uint32_t matchNumber;       // 4 bytes
} Message;

/* read big-endian integer up to 8 bytes */
static uint64_t read_be(const uint8_t *b, size_t nbytes) {
	uint64_t v = 0;
	for (size_t i = 0; i < nbytes; ++i) v = (v << 8) | b[i];
	return v;
}

/* Parse 6-byte big-endian timestamp into a 64-bit integer efficiently on little-endian machines like x86/arm64 */
static uint64_t parse_6byte_be_as_u64(const uint8_t *b) {
	uint64_t tmp;
	memcpy(&tmp, b, sizeof(tmp)); // read 8 bytes (may read two extra bytes past but caller must ensure buffer has room)
	tmp = __builtin_bswap64(tmp); // swap to host order
	return tmp >> 16; // drop low 2 bytes (we only needed top 6 bytes)
}

Message parseMessage(const unsigned char *msg, size_t len) {
	Message ee;
	memset(&ee, 0, sizeof ee);

	if (len < 11) return ee; // too short

	ee.messageType = (char)msg[0];
	ee.stockLocate = (uint16_t)read_be(msg + 1, 2);
	ee.trackingNumber = (uint16_t)read_be(msg + 3, 2);
	ee.timestamp = parse_6byte_be_as_u64(msg + 5);

	/* Following fields depend on message type and layout; example offsets below are for the C message you provided (Single Side Executed With Price)
	   Check and adjust offsets per ITTO/ITCH spec for other message types. */
	if (len >= 17) {
		ee.referenceNumber = read_be(msg + 11, 8);
	}
	if (len >= 21) {
		uint32_t t;
		memcpy(&t, msg + 19, 4); // example offset
		ee.executedContracts = ntohl(t);
	}
	if (len >= 25) {
		uint32_t t;
		memcpy(&t, msg + 23, 4);
		ee.crossNumber = ntohl(t);
	}
	if (len >= 29) {
		uint32_t t;
		memcpy(&t, msg + 27, 4);
		ee.matchNumber = ntohl(t);
	}

	return ee;
}

/* A small table for common message lengths (fill/adjust from spec); unknown types map to 0 */
static size_t message_length_by_type(unsigned char t) {
	switch (t) {
		case 'S': return 10; // System Event
		case 'R': return 44; // Options Directory
		case 'H': return 14; // Trading Action
		case 'O': return 14; // Option Open
		case 'a': return 26; // Add Order (Short)
		case 'A': return 30; // Add Order (Long)
		case 'j': return 37; // Add Quote (Short)
		case 'J': return 45; // Add Quote (Long)
		case 'E': return 29; // Single Side Executed
		case 'C': return 34; // Single Side Executed With Price
		case 'X': return 21; // Order Cancel
		case 'u': return 29; // Replace (Short)
		case 'U': return 33; // Replace (Long)
		case 'D': return 17; // Single Side Delete
		case 'G': return 26; // Single Side Update
		case 'k': return 49; // Quote Replace (Short)
		case 'K': return 57; // Quote Replace (Long)
		case 'Y': return 25; // Quote Delete
		case 'Q': return 30; // Cross Trade
		case 'I': return 35; // NOII
		default: return 0;
	}
}

int main() {
	unsigned char messageC[] = {
		0x43, 0x00, 0x01, 0x1F, 0x1A, 0xD9, 0x82, 0xB4, 0xD4,
		0x00, 0x00, 0x00, 0x00, 0xB2, 0xD1, 0x89, 0x14,
		0x00, 0x0F, 0x42, 0x40, 0x00, 0x4C, 0x4B, 0x48,
		0x4E, 0x00, 0x00, 0x44, 0xC0, 0x00, 0x00, 0x00, 0x01
	};

	unsigned char messageJ[] = {
		0x4A,0x00,0x00,0x1E,0xD5,0x01,0x12,0x20,0xA2,0x00,0x00,0x00,0x00,0xB3,0x28,0xA3,0xE4,0x00,0x00,0x00,0x00,0xB3,0x28,0xA3,0xE8,0x00,0x00,0xE4,0x10,0x00,0x66,0x14,0xD0,0x00,0x00,0x00,0x05,0x00,0x68,0x62,0xA8,0x00,0x00,0x00,0x05
	};

	mach_timebase_info_data_t timebase;
	mach_timebase_info(&timebase);

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

	double elapsed_ns = (double)(end - start) * (double)timebase.numer / (double)timebase.denom;
	double elapsed_per_iter = elapsed_ns / iterations;

	printf("Total elapsed time: %.4f ns\n", elapsed_ns);
	printf("Average per iteration: %.4f ns\n", elapsed_per_iter);

	if (dummy == 0) printf("");

	printf("\n--- Last Parsed Message (C then J) ---\n");
	printf("Message Type: %c\n", ee.messageType);
	printf("Stock Locate: %u\n", ee.stockLocate);
	printf("Tracking Number: %u\n", ee.trackingNumber);
	printf("Timestamp: %" PRIu64 "\n", ee.timestamp);
	printf("Reference Number: %" PRIu64 "\n", ee.referenceNumber);
	printf("Executed Contracts: %u\n", ee.executedContracts);
	printf("Cross Number: %u\n", ee.crossNumber);
	printf("Match Number: %u\n", ee.matchNumber);

	return 0;
}