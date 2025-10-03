\
import os, sys, asyncio, numpy as np, pandas as pd, time
from datetime import datetime, timezone
import argparse
from core import load_config, normalize_weights, synthetic_price, AlpacaBroker, is_rth, get_alpaca_keys
from core import StockDataStream

def bps(x): return x/1e4

def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default=os.environ.get("BASKET_CONFIG", "config_oih.yaml"))
    ap.add_argument("--feed", default="iex")
    return ap.parse_args()

async def run_once(cfg):
    weights = normalize_weights(cfg.basket.components)
    universe = list(weights.keys()) + ([cfg.basket.etf] if cfg.basket.etf else [])
    print("Universe:", universe)

    key, sec = get_alpaca_keys()
    stream = StockDataStream(key, sec)
    broker = AlpacaBroker(paper=True)

    closes = {sym: [] for sym in universe}
    last_px = {sym: None for sym in universe}
    initial_equity = broker.get_account_equity()
    max_equity = initial_equity
    print(f"Account equity baseline: {initial_equity:.2f}")

    def limit_from_last(last, side):
        if cfg.strategy.order_type.lower() == "market": return None
        slip = bps(cfg.strategy.limit_slip_bps)
        return last * (1 - slip) if side=="buy" else last * (1 + slip)

    async def cancel_open_after(delay_sec: int):
        await asyncio.sleep(max(delay_sec, 0))
        try:
            broker.cancel_all_open()
        except Exception as e:
            print("cancel_all_open error:", e)

    async def on_bar(bar):
        nonlocal max_equity
        # Guard: regular hours only
        ts = getattr(bar, "timestamp", None)
        if ts is None:
            ts = datetime.now(timezone.utc)
        if not is_rth(ts):
            return

        sym = bar.symbol
        px = float(bar.close)
        last_px[sym] = px
        closes[sym].append(px)
        if len(closes[sym]) > max(2000, cfg.strategy.lookback*5):
            closes[sym] = closes[sym][-cfg.strategy.lookback*5:]

        # Need enough history for all symbols
        if not all(len(closes[s]) >= cfg.strategy.lookback for s in universe):
            return

        df = pd.DataFrame({s: closes[s][-cfg.strategy.lookback*2:] for s in universe})
        syn = df[[c for c in weights]].apply(lambda row: sum(weights[t]*row[t] for t in weights), axis=1) * cfg.basket.multiplier
        if cfg.basket.etf:
            spread = df[cfg.basket.etf] - syn - cfg.basket.bias
        else:
            spread = syn - syn.rolling(cfg.strategy.lookback).mean()

        if len(spread) < cfg.strategy.lookback: return
        mu = spread.rolling(cfg.strategy.lookback).mean().iloc[-1]
        sd = spread.rolling(cfg.strategy.lookback).std(ddof=0).iloc[-1]
        if sd == 0 or np.isnan(sd): return
        z = float((spread.iloc[-1] - mu)/sd)
        print(f"[{datetime.now().isoformat(timespec='seconds')}] z={z:.2f} spread={spread.iloc[-1]:.4f}")

        # Kill switch using live equity
        try:
            eq = broker.get_account_equity()
            if eq > max_equity: max_equity = eq
            dd = (max_equity - eq) / max_equity if max_equity > 0 else 0.0
            if dd*100.0 >= cfg.risk.kill_switch_drawdown_pct:
                print(f"Kill switch: drawdown {dd*100:.2f}% >= {cfg.risk.kill_switch_drawdown_pct}% -> Flattening + cancel")
                broker.cancel_all_open()
                pos = broker.get_positions()
                for s in universe:
                    q = pos.get(s, 0.0)
                    if abs(q) < 1e-6: continue
                    side = "sell" if q > 0 else "buy"
                    last = last_px[s] if last_px[s] else None
                    lim = None if cfg.strategy.order_type.lower()=="market" else (last * (0.9995 if side=="buy" else 1.0005))
                    try:
                        broker.submit_limit_qty(symbol=s, side=side, notional=abs(q)*(last or 1.0), limit_price=(lim or last), tif=cfg.strategy.tif, cid_prefix="killswitch")
                    except Exception as e:
                        print("Flatten error:", s, e)
                raise SystemExit
        except Exception as e:
            print("Equity check error:", e)

        # Decide action
        act = None
        if z > cfg.strategy.entry_z:
            act = "SHORT"
        elif z < -cfg.strategy.entry_z:
            act = "LONG"
        elif abs(z) < cfg.strategy.exit_z:
            act = "EXIT"

        if act is None: return
        group_id = f"basket-{int(time.time())}"

        if act in ("LONG", "SHORT"):
            # Pair against ETF if provided
            leg_orders = []
            if cfg.basket.etf:
                etf_last = last_px[cfg.basket.etf]
                side_etf = "buy" if act=="LONG" else "sell"
                if cfg.strategy.order_type.lower() == "market":
                    leg_orders.append(("market_notional", cfg.basket.etf, side_etf, cfg.strategy.max_leg_notional, None))
                else:
                    lim = limit_from_last(etf_last, side_etf)
                    leg_orders.append(("limit_qty", cfg.basket.etf, side_etf, cfg.strategy.max_leg_notional, lim))

            leftover = max(cfg.strategy.max_total_notional - (cfg.strategy.max_leg_notional if cfg.basket.etf else 0), 0)
            for t, w in weights.items():
                ntl = (leftover if cfg.basket.etf else cfg.strategy.max_leg_notional) * w
                side = "sell" if act=="LONG" else "buy"
                last = last_px[t]
                if cfg.strategy.order_type.lower() == "market":
                    leg_orders.append(("market_notional", t, side, ntl, None))
                else:
                    lim = limit_from_last(last, side)
                    leg_orders.append(("limit_qty", t, side, ntl, lim))

            # Basic exposure cap: skip legs exceeding per-symbol cap
            for typ, sym, side, ntl, lim in leg_orders:
                if ntl > cfg.risk.max_symbol_notional: 
                    print(f"Skip {sym}: leg notional {ntl} > cap {cfg.risk.max_symbol_notional}")
                    continue
                try:
                    if typ == "market_notional":
                        broker.submit_notional_market(symbol=sym, side=side, notional=ntl, tif=cfg.strategy.tif, cid_prefix=group_id)
                    else:
                        broker.submit_limit_qty(symbol=sym, side=side, notional=ntl, limit_price=lim, tif=cfg.strategy.tif, cid_prefix=group_id)
                except Exception as e:
                    print("Order error:", sym, e)

            # Cancel any leftovers after a short delay (stale IOC/limits)
            asyncio.create_task(cancel_open_after(cfg.risk.cancel_after_sec))

        elif act == "EXIT":
            # Flatten live positions for all symbols in universe with qty
            try:
                pos = broker.get_positions()
                for sym in universe:
                    qty = pos.get(sym, 0.0)
                    if abs(qty) < 1e-6: continue
                    side = "sell" if qty > 0 else "buy"
                    last = last_px[sym]
                    lim = None if cfg.strategy.order_type.lower()=="market" else (last * (1 - bps(cfg.strategy.limit_slip_bps)) if side=="buy" else last * (1 + bps(cfg.strategy.limit_slip_bps)))
                    try:
                        broker.submit_limit_qty(symbol=sym, side=side, notional=abs(qty)*last, limit_price=(lim or last), tif=cfg.strategy.tif, cid_prefix=f"{group_id}-exit")
                    except Exception as e:
                        print("Exit order error:", sym, e)
            except Exception as e:
                print("Exit error:", e)

    # Subscribe to minute bars
    for s in universe:
        stream.subscribe_bars(on_bar, s)

    await stream.run()

async def main():
    args = parse_args()
    cfg = load_config(args.config)
    # Reconnect loop with backoff
    delay = 1
    while True:
        try:
            await run_once(cfg)
        except SystemExit:
            print("Stopped by kill switch.")
            return
        except Exception as e:
            print("Stream/run error:", e)
            await asyncio.sleep(delay)
            delay = min(delay * 2, 30)  # exponential backoff cap 30s

if __name__ == "__main__":
    asyncio.run(main())
