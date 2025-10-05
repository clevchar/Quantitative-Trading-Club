# ITCH Playground — minimal starter

This small starter includes a minimal ITCH parser (prints Add messages) and a simple UDP simulator (replays raw bytes). It's intended as a baseline; extend the parser to support the full NASDAQ ITCH spec.

Download sample NASDAQ ITCH files from: https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/

Quick build (macOS, zsh):

```bash
cd C++\ ITCH\ stream\ parser/itch-playground 
gunzip -k "./01302020.NASDAQ_ITCH50.gz"  # keep original
mkdir -p itch-playground/build
cd itch-playground/build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

Run parser (prints Add messages as CSV):

```bash
./parser/parser_bin /absolute/path/to/decompressed_sample
```

Run simulator (send UDP to localhost:9000 in burst mode):

```bash
./simulator/simulator_bin /absolute/path/to/decompressed_sample 127.0.0.1 9000 --burst 1400
```

Notes:
- This parser is intentionally minimal and only decodes a simplified 'A' Add message for demonstration. Consult the official NASDAQ TotalView–ITCH spec for exact field layouts and endianness.
- The simulator sends raw chunks as UDP datagrams. For realistic timing use timestamps inside messages to pace sends.
