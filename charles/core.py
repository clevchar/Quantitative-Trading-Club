\
from __future__ import annotations
import os, time, math, uuid, asyncio, dataclasses, contextlib
from dataclasses import dataclass
from typing import Dict, List, Tuple, Optional
import numpy as np
import pandas as pd
import yaml
import pytz
from datetime import datetime, timezone, timedelta

# --- Optional .env loading (safe no-op if the package isn't present) ---
with contextlib.suppress(Exception):
    from dotenv import load_dotenv  # type: ignore
    load_dotenv()

# --- Cautious key retrieval: env first, otherwise placeholders (EDIT LOCALLY if desired) ---
ALPACA_API_KEY    = os.getenv("ALPACA_API_KEY")    or "YOUR_KEY"
ALPACA_SECRET_KEY = os.getenv("ALPACA_SECRET_KEY") or "YOUR_SECRET"

def get_alpaca_keys() -> Tuple[str, str]:
    return ALPACA_API_KEY, ALPACA_SECRET_KEY

# ---- Alpaca SDK imports (import-time safe) ----
with contextlib.suppress(Exception):
    from alpaca.data.requests import StockBarsRequest
    from alpaca.data.timeframe import TimeFrame
    from alpaca.data.enums import DataFeed
    from alpaca.data.historical import StockHistoricalDataClient
    from alpaca.data.live import StockDataStream
    from alpaca.trading.client import TradingClient
    from alpaca.trading.requests import MarketOrderRequest, LimitOrderRequest, GetOrdersRequest
    from alpaca.trading.enums import OrderSide, TimeInForce, QueryOrderStatus

# ------------------- Config -------------------

@dataclass
class BasketConfig:
    name: str
    etf: Optional[str]
    components: Dict[str, float]
    bias: float = 0.0
    multiplier: float = 1.0

@dataclass
class StrategyConfig:
    timeframe: str = "1Min"
    lookback: int = 120
    entry_z: float = 1.5
    exit_z: float  = 0.25
    max_leg_notional: float = 4000.0
    max_total_notional: float = 20000.0
    side: str = "pairs"  # "pairs" | "arb"
    order_type: str = "limit"  # "market" | "limit"
    limit_slip_bps: float = 5.0
    tif: str = "ioc"
    rebalance_each_bar: bool = False

@dataclass
class RiskConfig:
    allow_shorts: bool = True
    max_gross_exposure: float = 50000.0
    max_symbol_notional: float = 15000.0
    cancel_after_sec: int = 3
    kill_switch_drawdown_pct: float = 5.0

@dataclass
class DataConfig:
    start: str
    end: str

@dataclass
class AppConfig:
    basket: BasketConfig
    strategy: StrategyConfig
    risk: RiskConfig
    data: DataConfig

def load_config(path: str) -> AppConfig:
    with open(path, "r") as f:
        raw = yaml.safe_load(f)
    b = raw["basket"]; s = raw["strategy"]; r = raw["risk"]; d = raw["data"]
    return AppConfig(
        basket=BasketConfig(
            name=b["name"],
            etf=b.get("etf"),
            components=b["components"],
            bias=float(b.get("bias", 0.0)),
            multiplier=float(b.get("multiplier", 1.0)),
        ),
        strategy=StrategyConfig(**s),
        risk=RiskConfig(**r),
        data=DataConfig(**d),
    )

# ------------------- Basket math -------------------

def normalize_weights(w: Dict[str, float]) -> Dict[str, float]:
    arr = np.array(list(w.values()), dtype=float)
    s = arr.sum()
    if s == 0: raise ValueError("Weights sum to zero.")
    return {k: float(v) / float(s) for k, v in w.items()}

def synthetic_price(row: pd.Series, weights: Dict[str, float], mult: float = 1.0) -> float:
    val = 0.0
    for t, w in weights.items():
        val += w * float(row[t])
    return float(mult * val)

def zscore(x: pd.Series, lookback: int) -> pd.Series:
    mu = x.rolling(lookback, min_periods=lookback//2).mean()
    sd = x.rolling(lookback, min_periods=lookback//2).std(ddof=0)
    return (x - mu) / sd

# ------------------- Data helpers -------------------

_TF_MAP = {}
with contextlib.suppress(Exception):
    _TF_MAP = {
        "1Min": TimeFrame.Minute,
        "5Min": TimeFrame(5, TimeFrame.Unit.Minute),
        "15Min": TimeFrame(15, TimeFrame.Unit.Minute),
        "1H": TimeFrame.Hour,
        "1D": TimeFrame.Day,
    }

def get_hist_bars(
    symbols: List[str],
    start: str,
    end: str,
    timeframe: str = "1Min",
    feed: str = "iex",
) -> pd.DataFrame:
    \"\"\"Download historical bars from Alpaca (multi-symbol, returns wide df of 'close').\"\"\"
    key, sec = get_alpaca_keys()
    client = StockHistoricalDataClient(key, sec)
    req = StockBarsRequest(symbol_or_symbols=symbols, start=start, end=end, timeframe=_TF_MAP[timeframe], feed=DataFeed[feed.upper()])
    bars = client.get_stock_bars(req).df  # multi-index: (symbol, timestamp)
    close = bars["close"].unstack(level=0).sort_index()
    close.index.name = "timestamp"
    return close

# ------------------- Strategy logic -------------------

@dataclass
class Signal:
    spread: float
    z: float
    action: str     # "LONG_BASKET" | "SHORT_BASKET" | "EXIT" | "NONE"
    target: Dict[str, float]  # target notional per leg (signed)

def make_signal(
    now_prices: Dict[str, float],
    hist_spread: pd.Series,
    cfg: AppConfig,
) -> Signal:
    if len(hist_spread) < max(10, cfg.strategy.lookback//2) or hist_spread.std() == 0:
        return Signal(0.0, 0.0, "NONE", {})
    roll = cfg.strategy.lookback
    mu = hist_spread.rolling(roll).mean().iloc[-1]
    sd = hist_spread.rolling(roll).std(ddof=0).iloc[-1]
    if sd == 0 or pd.isna(sd) or pd.isna(mu):
        return Signal(float(hist_spread.iloc[-1]), 0.0, "NONE", {})
    z = float((hist_spread.iloc[-1] - mu) / sd)

    act = "NONE"; targets: Dict[str, float] = {}
    b = cfg.basket; s = cfg.strategy
    weights = normalize_weights(b.components)

    if s.side == "pairs" and b.etf:
        if z > s.entry_z:
            act = "SHORT_BASKET"
            targets[b.etf] = -s.max_leg_notional
            leftover = max(s.max_total_notional - abs(targets[b.etf]), 0.0)
            for tkr, w in weights.items():
                targets[tkr] = +leftover * w
        elif z < -s.entry_z:
            act = "LONG_BASKET"
            targets[b.etf] = +s.max_leg_notional
            leftover = max(s.max_total_notional - abs(targets[b.etf]), 0.0)
            for tkr, w in weights.items():
                targets[tkr] = -leftover * w
        elif abs(z) < s.exit_z:
            act = "EXIT"
    else:
        if z > s.entry_z:
            act = "SHORT_BASKET"
            for tkr, w in weights.items():
                targets[tkr] = -s.max_leg_notional * w
        elif z < -s.entry_z:
            act = "LONG_BASKET"
            for tkr, w in weights.items():
                targets[tkr] = +s.max_leg_notional * w
        elif abs(z) < s.exit_z:
            act = "EXIT"

    return Signal(float(hist_spread.iloc[-1]), z, act, targets)

# ------------------- Backtest engine -------------------

@dataclass
class Fill:
    ts: pd.Timestamp
    symbol: str
    qty: float
    px: float

@dataclass
class Position:
    qty: float = 0.0

class Backtester:
    def __init__(self, cfg: AppConfig):
        self.cfg = cfg
        self.positions: Dict[str, Position] = {}
        self.cash: float = 0.0
        self.fills: List[Fill] = []

    def _price_to_qty(self, notional: float, price: float) -> float:
        if price <= 0: return 0.0
        return notional / price

    def _exec_order(self, ts: pd.Timestamp, symbol: str, target_notional_delta: float, price: float, slip_bps: float = 1.0):
        side = 1 if target_notional_delta > 0 else -1
        px = price * (1 + side * slip_bps/1e4)
        qty = self._price_to_qty(abs(target_notional_delta), px) * side
        self.positions.setdefault(symbol, Position())
        self.positions[symbol].qty += qty
        self.cash -= qty * px
        self.fills.append(Fill(ts, symbol, qty, px))

    def run(self, close_prices: pd.DataFrame, etf: Optional[str], weights: Dict[str, float], bias: float, mult: float) -> pd.DataFrame:
        comp_cols = list(weights.keys())
        df = close_prices.copy().dropna(axis=0, how="any", subset=(comp_cols + ([etf] if etf else [])))
        syn = df[comp_cols].apply(lambda row: synthetic_price(row, weights, mult), axis=1)
        if etf:
            spread_series = df[etf] - syn - bias
        else:
            spread_series = syn - syn.rolling(self.cfg.strategy.lookback).mean()

        equity_curve = []
        for i in range(1, len(df)):
            ts = df.index[i]
            hist_spread = spread_series.iloc[:i+1]
            now_row = df.iloc[i]
            now_prices = now_row.to_dict()

            sig = make_signal(now_prices, hist_spread, self.cfg)

            if sig.action in ("LONG_BASKET", "SHORT_BASKET"):
                gross = sum(abs(v) for v in sig.target.values())
                scale = min(1.0, self.cfg.strategy.max_total_notional / gross) if gross > 0 else 1.0
                for sym, ntl in sig.target.items():
                    px = now_prices[sym]
                    self._exec_order(ts, sym, ntl*scale, px, slip_bps=1.0)
            elif sig.action == "EXIT":
                for sym, pos in list(self.positions.items()):
                    px = now_prices.get(sym, np.nan)
                    if np.isnan(px) or pos.qty == 0: continue
                    self._exec_order(ts, sym, -pos.qty*px, px, slip_bps=1.0)

            # MTM
            eq = self.cash
            for sym, pos in self.positions.items():
                px = now_prices.get(sym, np.nan)
                if not np.isnan(px):
                    eq += pos.qty * px
            equity_curve.append((ts, eq))

        return pd.DataFrame(equity_curve, columns=["timestamp", "equity"]).set_index("timestamp")

# ------------------- Alpaca broker helpers -------------------

class AlpacaBroker:
    \"\"\"Wrapper for notional (market) and qty (limit) orders, positions, account equity, and order mgmt.\"\"\"
    def __init__(self, paper: bool = True):
        key, sec = get_alpaca_keys()
        self.trading = TradingClient(key, sec, paper=paper)

    def _tif(self, tif: str) -> TimeInForce:
        return getattr(TimeInForce, tif.upper())

    # Market: prefer notional (fractional-friendly)
    def submit_notional_market(self, symbol: str, side: str, notional: float, tif: str, cid_prefix: str):
        side_enum = OrderSide.BUY if (side.lower()=="buy") else OrderSide.SELL
        client_order_id = f"{cid_prefix}-{symbol}-{uuid.uuid4().hex[:8]}"
        from alpaca.trading.requests import MarketOrderRequest
        req = MarketOrderRequest(symbol=symbol, notional=abs(float(notional)), side=side_enum, time_in_force=self._tif(tif))
        return self.trading.submit_order(req, client_order_id=client_order_id)

    # Limit: compute qty from notional
    def submit_limit_qty(self, symbol: str, side: str, notional: float, limit_price: float, tif: str, cid_prefix: str):
        side_enum = OrderSide.BUY if (side.lower()=="buy") else OrderSide.SELL
        client_order_id = f"{cid_prefix}-{symbol}-{uuid.uuid4().hex[:8]}"
        qty = max(int(abs(float(notional)) / float(limit_price)), 1)
        from alpaca.trading.requests import LimitOrderRequest
        req = LimitOrderRequest(symbol=symbol, qty=qty, side=side_enum, time_in_force=self._tif(tif), limit_price=float(limit_price))
        return self.trading.submit_order(req, client_order_id=client_order_id)

    def cancel_all_open(self):
        self.trading.cancel_orders()

    def open_orders(self):
        return self.trading.get_orders(filter=GetOrdersRequest(status=QueryOrderStatus.OPEN))

    def get_positions(self) -> Dict[str, float]:
        pos = {}
        for p in self.trading.get_all_positions():
            qty = float(p.qty)
            sym = p.symbol
            pos[sym] = qty
        return pos

    def get_account_equity(self) -> float:
        acct = self.trading.get_account()
        try:
            return float(acct.equity)
        except Exception:
            return float(acct.cash)

# ------------------- Trading-time helpers -------------------
NY = pytz.timezone("America/New_York")

def is_rth(dt_utc: datetime) -> bool:
    if dt_utc.tzinfo is None:
        dt_utc = dt_utc.replace(tzinfo=timezone.utc)
    dt_ny = dt_utc.astimezone(NY)
    if dt_ny.weekday() >= 5:  # Sat/Sun
        return False
    start = dt_ny.replace(hour=9, minute=30, second=0, microsecond=0)
    end   = dt_ny.replace(hour=16, minute=0,  second=0, microsecond=0)
    return start <= dt_ny <= end
