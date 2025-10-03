\
import os, sys, pandas as pd, argparse
from core import load_config, normalize_weights, get_hist_bars, Backtester

def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default=os.environ.get("BASKET_CONFIG", "config_oih.yaml"))
    ap.add_argument("--feed", default="iex")
    return ap.parse_args()

def main():
    args = parse_args()
    cfg = load_config(args.config)
    comps = list(cfg.basket.components.keys())
    symbols = comps + ([cfg.basket.etf] if cfg.basket.etf else [])
    print(f"Fetching bars for: {symbols}")
    px = get_hist_bars(symbols, cfg.data.start, cfg.data.end, cfg.strategy.timeframe, feed=args.feed)
    weights = normalize_weights(cfg.basket.components)

    bt = Backtester(cfg)
    equity = bt.run(px, cfg.basket.etf, weights, cfg.basket.bias, cfg.basket.multiplier)
    equity.to_csv("backtest_equity.csv")
    print("Done. Wrote backtest_equity.csv")

if __name__ == "__main__":
    main()
