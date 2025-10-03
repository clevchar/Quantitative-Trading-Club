Basket Trading (OIH) — Paper Trading & Backtest (v4, cautious keys)
==================================================================

Credentials
-----------
This bundle reads keys from env vars OR falls back to placeholders in code:
- ALPACA_API_KEY    = os.getenv("ALPACA_API_KEY")    or "YOUR_KEY"
- ALPACA_SECRET_KEY = os.getenv("ALPACA_SECRET_KEY") or "YOUR_SECRET"

Options to provide keys:
1) Export env vars (recommended):
   export ALPACA_API_KEY="..."
   export ALPACA_SECRET_KEY="..."

2) Create a .env file (loaded automatically if python-dotenv is present):
   ALPACA_API_KEY=...
   ALPACA_SECRET_KEY=...

3) Edit core.py and replace "YOUR_KEY"/"YOUR_SECRET" with your real keys (least safe).

Install
-------
pip install -r requirements.txt

Backtest
--------
python backtest.py --config config_oih.yaml --feed iex
-> writes backtest_equity.csv

Paper Trading
-------------
python live_paper.py --config config_oih.yaml

Safety features
---------------
- RTH guard (9:30–16:00 ET, weekdays)
- Market notional orders (fractional) or limit qty-from-notional
- IOC limits with ±5 bps slip (configurable)
- Auto-cancel stale orders after N seconds
- Kill switch based on account equity drawdown
- Websocket auto-reconnect
