"""
OHLCV price data loader for the ModernQuantSuite Research Dashboard.

Priority order:
1. Local C++ exported JSON candles in `modernquant/reports/ohlcv/<TICKER>.json`
   (fast, zero-latency offline support for BIST / XETRA / US / synthetic panels)
2. Live fallback via yfinance if missing locally
"""

import json
import logging
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional

logger = logging.getLogger(__name__)

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
LOCAL_OHLCV_DIR = PROJECT_ROOT / "reports" / "ohlcv"


def _load_local_ohlcv(ticker: str) -> Optional[List[Dict[str, Any]]]:
    safe = ticker.replace("/", "_").replace("\\", "_")
    candidates = [
        LOCAL_OHLCV_DIR / f"{safe}.json",
        LOCAL_OHLCV_DIR / f"{safe.upper()}.json",
    ]
    for path in candidates:
        if path.exists():
            try:
                with open(path, "r", encoding="utf-8") as f:
                    records = json.load(f)
                if isinstance(records, list) and records:
                    return records
            except Exception as exc:
                logger.warning("Failed reading local OHLCV cache %s: %s", path, exc)
    return None


def _download_yfinance(ticker: str, start: str, end: str) -> List[Dict[str, Any]]:
    try:
        import yfinance as yf
        import pandas as pd

        df = yf.download(ticker, start=start, end=end, progress=False)
        if df is None or df.empty:
            return []
        if isinstance(df.columns, pd.MultiIndex):
            df.columns = df.columns.get_level_values(0)

        records: List[Dict[str, Any]] = []
        for idx, row in df.iterrows():
            try:
                ts = idx
                if hasattr(ts, "tz") and ts.tz is not None:
                    ts = ts.tz_localize(None)
                records.append({
                    "time": ts.strftime("%Y-%m-%d"),
                    "open": round(float(row.get("Open", row.get("open", 0))), 4),
                    "high": round(float(row.get("High", row.get("high", 0))), 4),
                    "low": round(float(row.get("Low", row.get("low", 0))), 4),
                    "close": round(float(row.get("Close", row.get("close", 0))), 4),
                    "volume": int(row.get("Volume", row.get("volume", 0))),
                })
            except (TypeError, ValueError):
                continue
        return records
    except Exception as exc:
        logger.warning("yfinance fallback unavailable for %s: %s", ticker, exc)
        return []


def get_ohlcv(
    ticker: str,
    start: Optional[str] = None,
    end: Optional[str] = None,
) -> List[Dict[str, Any]]:
    records = _load_local_ohlcv(ticker)
    if records is None:
        s = start or "2023-01-01"
        e = end or datetime.now().strftime("%Y-%m-%d")
        records = _download_yfinance(ticker, s, e)

    if not records:
        return []

    if start:
        records = [r for r in records if str(r.get("time", "")) >= start]
    if end:
        records = [r for r in records if str(r.get("time", "")) <= end]

    return records


def get_available_tickers() -> List[str]:
    if not LOCAL_OHLCV_DIR.exists():
        return []
    return sorted(p.stem for p in LOCAL_OHLCV_DIR.glob("*.json"))
