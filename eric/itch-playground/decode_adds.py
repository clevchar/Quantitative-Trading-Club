#!/usr/bin/env python3
# decode_adds.py
# Usage: python3 decode_adds.py /path/to/01302020.NASDAQ_ITCH50 [max_rows]
import sys
import os
import re

def be_uint(b):
    v = 0
    for x in b:
        v = (v << 8) | x
    return v

def csv_escape(s):
    s = str(s)
    if any(c in s for c in (',', '"', '\n', '\r')):
        s = '"' + s.replace('"', '""') + '"'
    return s

def decode_add_at(buf, pos):
    # pos points at the 'A' byte
    # layout: 1 (type) + 2 (stockLocate) + 2 (tracking) + 6 (timestamp)
    #         + 8 (orderRef) + 1 (side) + 4 (shares) + 8 (stock) + 4 (price)
    header_len = 1 + 2 + 2 + 6
    payload_len = 8 + 1 + 4 + 8 + 4
    need = header_len + payload_len
    if pos + need > len(buf):
        return None
    cur = pos + 1
    stock_loc = be_uint(buf[cur:cur+2]); cur += 2
    track_num = be_uint(buf[cur:cur+2]); cur += 2
    ts48 = be_uint(buf[cur:cur+6]); cur += 6  # nanoseconds since midnight (48-bit)
    order_ref = be_uint(buf[cur:cur+8]); cur += 8
    side = chr(buf[cur]) if buf[cur] >= 32 and buf[cur] <= 126 else '?'; cur += 1
    shares = be_uint(buf[cur:cur+4]); cur += 4
    stock = buf[cur:cur+8].decode('ascii', errors='replace').rstrip('\x00').rstrip(); cur += 8
    price_int = be_uint(buf[cur:cur+4]); cur += 4
    # Convert timestamp (48-bit ns) to human time (assume date unknown => show hh:mm:ss.ssssss)
    seconds = ts48 // 1_000_000_000
    ns_rem = ts48 % 1_000_000_000
    hh = seconds // 3600
    mm = (seconds % 3600) // 60
    ss = seconds % 60
    ts_human = f"{hh:02d}:{mm:02d}:{ss:02d}.{ns_rem:09d}"
    # price scaling: many ITCH feeds use price in 1/10000 ($ = price/10000). adjust scale if needed.
    price = price_int / 10000.0
    return {
        'ts48': ts48,
        'ts_human': ts_human,
        'stock_loc': stock_loc,
        'track_num': track_num,
        'order_ref': order_ref,
        'side': side,
        'shares': shares,
        'stock': stock,
        'price_int': price_int,
        'price': price,
        'next_pos': cur
    }

def scan_file(path, max_rows=2000):
    with open(path, 'rb') as f:
        buf = f.read()
    i = 0
    found = 0
    n = len(buf)
    # try to infer date from filename (MMDDYYYY) to form full timestamps
    basename = os.path.basename(path)
    date_prefix = None
    m = re.match(r"(\d{8})", basename)
    if m:
        s = m.group(1)
        # assume MMDDYYYY
        try:
            mm = int(s[0:2]); dd = int(s[2:4]); yyyy = int(s[4:8])
            date_prefix = f"{yyyy:04d}-{mm:02d}-{dd:02d}T"
        except Exception:
            date_prefix = None

    # print CSV header
    print('timestamp,order_ref,side,shares,price,stock')

    while i < n:
        try:
            pos = buf.index(b'A', i)
        except ValueError:
            break
        decoded = decode_add_at(buf, pos)
        if decoded:
            # sanitize stock symbol: keep printable alphanum, dot, dash, space
            stock = decoded['stock']
            stock = stock.replace("\x00", "").strip()
            stock = re.sub(r"[^A-Z0-9.\- ]", "", stock.upper())

            # attach date if available
            ts = decoded['ts_human']
            if date_prefix:
                ts = date_prefix + ts

            # CSV-safe print: escape fields
            out = [ts, str(decoded['order_ref']), decoded['side'], str(decoded['shares']), f"{decoded['price']:.4f}", stock]
            out = [csv_escape(x) for x in out]
            print(','.join(out))
            found += 1
            if found >= max_rows:
                break
            i = decoded['next_pos']
        else:
            i = pos + 1

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: python3 decode_adds.py /path/to/decompressed_file [max_rows]')
        sys.exit(1)
    path = sys.argv[1]
    max_rows = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    scan_file(path, max_rows)