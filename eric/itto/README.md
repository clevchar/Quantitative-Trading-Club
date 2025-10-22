# NASDAQ ITTO Parser

A high-performance C parser for NASDAQ ITTO (OUCH Trade Output) market data messages.

## Overview

This parser handles all 19 ITTO message types defined in the [NASDAQ ITTO specification](https://www.nasdaqtrader.com/content/productsservices/trading/optionsmarket/itto_spec40.pdf):

- **S** - System Event
- **R** - Options Directory  
- **H** - Trading Action
- **O** - Option Open
- **a** - Add Order (Short)
- **A** - Add Order (Long)
- **j** - Add Quote (Short)
- **J** - Add Quote (Long)
- **E** - Single Side Executed
- **C** - Single Side Executed With Price
- **X** - Order Cancel
- **u** - Replace (Short)
- **U** - Replace (Long)
- **D** - Single Side Delete
- **G** - Single Side Update
- **k** - Quote Replace (Short)
- **K** - Quote Replace (Long)
- **Y** - Quote Delete
- **Q** - Cross Trade
- **I** - NOII (Net Order Imbalance Indicator)

## How It Works

### Message Structure

Every ITTO message follows this pattern:

```
[Message Type: 1 byte][Header Fields][Message-Specific Payload]
```

Most messages share a common header:

```c
typedef struct {
    char messageType;        // 1 byte - ASCII letter identifying message type
    uint16_t stockLocate;    // 2 bytes - stock/option locate code
    uint16_t trackingNumber; // 2 bytes - tracking sequence number
    uint64_t timestamp;      // 6 bytes - nanoseconds since midnight
} ITTOHeader;
```

### Big-Endian Parsing

ITTO uses network byte order (big-endian). The parser provides optimized readers:

```c
// Read 2-byte big-endian integer
uint16_t read_u16(const uint8_t *b) {
    return (uint16_t)((b[0] << 8) | b[1]);
}

// Read 4-byte big-endian integer
uint32_t read_u32(const uint8_t *b) {
    return (uint32_t)((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
}

// Read 8-byte big-endian integer
uint64_t read_u64(const uint8_t *b) {
    return read_be(b, 8);
}
```

### 6-Byte Timestamp Trick

Timestamps are 6 bytes (48 bits) but modern CPUs work efficiently with 8-byte (64-bit) values. The parser uses a fast trick:

```c
static inline uint64_t parse_timestamp(const uint8_t *b) {
    uint64_t tmp;
    memcpy(&tmp, b, 8);              // Read 8 bytes (includes 2 extra)
    tmp = __builtin_bswap64(tmp);     // Swap all 8 bytes to host order
    return tmp >> 16;                 // Shift right 16 bits to drop 2 bytes
}
```

**Why this works:**
1. Reading 8 bytes at once is fast (single memory operation)
2. `__builtin_bswap64` is a single CPU instruction on modern architectures
3. Right shift by 16 bits discards the 2 extra bytes we didn't need
4. Result: ~0.00 ns per parse (essentially free in tight loops)

**Important:** Ensure your input buffer has at least 8 bytes available from the timestamp start position, or copy into a local zeroed 8-byte buffer first.

### Type-Specific Parsing

Each message type has a dedicated parser function that knows the exact field layout:

```c
/* [J] Add Quote (Long) - 45 bytes */
static void parse_J(const uint8_t *msg, size_t len) {
    if (len < 45) return;
    
    ITTOHeader h = parse_header(msg);            // bytes 0-10
    uint64_t bidRefNum = read_u64(msg + 11);     // bytes 11-18
    uint64_t askRefNum = read_u64(msg + 19);     // bytes 19-26
    uint32_t bidSize = read_u32(msg + 27);       // bytes 27-30
    uint32_t askSize = read_u32(msg + 31);       // bytes 31-34
    uint32_t optionId = read_u32(msg + 35);      // bytes 35-38
    uint32_t bidPrice = read_u32(msg + 39);      // bytes 39-42
    uint32_t askPrice = read_u32(msg + 43);      // bytes 43-46
    
    // Use the parsed fields...
}
```

### Dispatcher Pattern

The main parser uses a switch statement for fast dispatch:

```c
void parse_message(const uint8_t *msg, size_t len) {
    if (len == 0) return;
    char type = (char)msg[0];
    
    switch (type) {
        case 'S': parse_S(msg, len); break;
        case 'R': parse_R(msg, len); break;
        case 'J': parse_J(msg, len); break;
        // ... all 19 types
        default:
            printf("Unknown message type: %c\n", type);
    }
}
```

## Adding Support for New Message Types

To add or modify a message parser:

1. **Check the ITTO spec** for the exact message layout and byte offsets
2. **Determine message length** - all ITTO messages have fixed lengths
3. **Create a parse function** following this template:

```c
/* [X] Your Message Type (N bytes) */
static void parse_X(const uint8_t *msg, size_t len) {
    if (len < N) return;  // Safety check
    
    ITTOHeader h = parse_header(msg);
    
    // Extract fields at exact offsets per spec
    uint32_t field1 = read_u32(msg + 11);
    uint64_t field2 = read_u64(msg + 15);
    // etc...
    
    // Process the message
    printf("[X] Your Message Type\n");
    printf("  Field1: %u\n", field1);
}
```

4. **Add to dispatcher** in `parse_message()`:
```c
case 'X': parse_X(msg, len); break;
```

## Message Length Table

Use this table to validate message boundaries in a stream:

| Type | Name                          | Length |
|------|-------------------------------|--------|
| S    | System Event                  | 10     |
| R    | Options Directory             | 44     |
| H    | Trading Action                | 14     |
| O    | Option Open                   | 14     |
| a    | Add Order (Short)             | 26     |
| A    | Add Order (Long)              | 30     |
| j    | Add Quote (Short)             | 37     |
| J    | Add Quote (Long)              | 45     |
| E    | Single Side Executed          | 29     |
| C    | Single Side Executed w/ Price | 34     |
| X    | Order Cancel                  | 21     |
| u    | Replace (Short)               | 29     |
| U    | Replace (Long)                | 33     |
| D    | Single Side Delete            | 17     |
| G    | Single Side Update            | 26     |
| k    | Quote Replace (Short)         | 49     |
| K    | Quote Replace (Long)          | 57     |
| Y    | Quote Delete                  | 25     |
| Q    | Cross Trade                   | 30     |
| I    | NOII                          | 35     |

## Building

```bash
make           # Build optimized binaries
make clean     # Clean build artifacts
make debug     # Build with debug symbols
```

## Running

```bash
./itto_parser  # Parse all 19 test messages and show performance
./deciphering  # Original header parsing example
```

## Performance

The parser is optimized for high-throughput market data:

- **Header parsing**: ~0.00 ns per message (measured on Apple Silicon)
- **Full message dispatch**: ~1-2 ns per message
- Zero allocations - all stack-based
- Branchless big-endian conversion using compiler intrinsics
- Cache-friendly sequential access patterns

## Example: Parsing a Live Stream

```c
// In your feed handler
while (read_from_socket(buffer, &bytes_read)) {
    size_t offset = 0;
    
    while (offset + 1 <= bytes_read) {
        uint8_t msg_type = buffer[offset];
        size_t msg_len = get_message_length(msg_type);
        
        if (offset + msg_len > bytes_read) {
            // Incomplete message, wait for more data
            memmove(buffer, buffer + offset, bytes_read - offset);
            bytes_read = bytes_read - offset;
            break;
        }
        
        parse_message(buffer + offset, msg_len);
        offset += msg_len;
    }
}
```

## Common Field Types

### Price Fields
Prices are encoded as integers representing ticks. Check the spec for the scaling factor (typically 1/10000 for decimal representation):

```c
uint32_t price_raw = read_u32(msg + offset);
double price_dollars = price_raw / 10000.0;
```

### Size/Quantity Fields
Sizes are straightforward unsigned integers (contracts/shares):

```c
uint32_t size = read_u32(msg + offset);
```

### ASCII Fields
Symbol names, exchange codes, etc. are fixed-width ASCII padded with spaces:

```c
char symbol[9];
read_ascii(msg + offset, 6, symbol, sizeof(symbol));  // Read 6 bytes, NUL-terminate
```

### Reference Numbers
Order/quote reference numbers are 8-byte unique identifiers:

```c
uint64_t ref_num = read_u64(msg + offset);
```

## Testing

The `itto_parser.c` file includes test cases for all 19 message types using real market data hex dumps. Run it to verify parsing correctness.

## References

- [NASDAQ ITTO Specification v4.0](https://www.nasdaqtrader.com/content/productsservices/trading/optionsmarket/itto_spec40.pdf)
- [NASDAQ Market Data Information](https://www.nasdaqtrader.com/)
