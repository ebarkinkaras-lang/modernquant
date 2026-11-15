"""
ModernQuantSuite Research Dashboard — FastAPI Backend Server.

Serves cross-sectional portfolio experiments, fake-alpha diagnostics,
per-stock predictability tables, and OHLCV chart candles exported by C++20.
"""

from pathlib import Path
from typing import Optional

from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from .data_loader import (
    REPORTS_DIR,
    get_cross_sectional_ic,
    get_experiment_detail,
    get_holdings,
    get_ticker_rankings,
    get_trades,
    list_experiments,
)
from .price_loader import get_available_tickers, get_ohlcv

app = FastAPI(
    title="ModernQuantSuite C++20 Research Dashboard API",
    description="API for Cross-Sectional Portfolio Backtests, Sticky Sentiment, Fake-Alpha Diagnostics & Predictability",
    version="2.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/api/health")
def health():
    return {
        "status": "ok",
        "engine": "ModernQuantSuite C++20 + FastAPI",
        "reports_dir": str(REPORTS_DIR),
        "reports_exists": REPORTS_DIR.exists(),
    }


@app.get("/api/experiments")
def api_list_experiments():
    return {"experiments": list_experiments()}


@app.get("/api/experiment/{name}")
def api_get_experiment(name: str):
    detail = get_experiment_detail(name)
    if detail is None:
        raise HTTPException(status_code=404, detail=f"Experiment '{name}' not found")
    return detail


@app.get("/api/experiment/{name}/ic")
def api_get_ic(name: str):
    series = get_cross_sectional_ic(name)
    if not series:
        raise HTTPException(status_code=404, detail=f"No IC time series for '{name}'")
    return {"name": name, "ic_series": series}


@app.get("/api/experiment/{name}/trades/{strategy}")
def api_get_trades(name: str, strategy: str):
    return {
        "name": name,
        "strategy": strategy,
        "trades": get_trades(name, strategy),
        "holdings": get_holdings(name, strategy),
    }


@app.get("/api/experiment/{name}/ticker-rankings/{strategy}")
def api_get_ticker_rankings(name: str, strategy: str):
    return {"name": name, "strategy": strategy, "rankings": get_ticker_rankings(name, strategy)}


@app.get("/api/compare")
def api_compare(exp1: str = Query(...), exp2: str = Query(...)):
    d1 = get_experiment_detail(exp1)
    d2 = get_experiment_detail(exp2)
    if d1 is None or d2 is None:
        missing = exp1 if d1 is None else exp2
        raise HTTPException(status_code=404, detail=f"Experiment '{missing}' not found")
    return {"exp1": d1, "exp2": d2}


@app.get("/api/price/{ticker}")
def api_get_price(
    ticker: str,
    start: Optional[str] = Query(None),
    end: Optional[str] = Query(None),
):
    candles = get_ohlcv(ticker, start=start, end=end)
    if not candles:
        raise HTTPException(status_code=404, detail=f"No OHLCV price data for '{ticker}'")
    return {"ticker": ticker, "candles": candles, "count": len(candles)}


@app.get("/api/tickers")
def api_list_tickers():
    tickers = get_available_tickers()
    return {"tickers": tickers, "count": len(tickers)}


# Serve built production React bundle automatically when visiting http://localhost:8000
_DIST_DIR = Path(__file__).resolve().parent.parent / "frontend" / "dist"
if _DIST_DIR.exists():
    app.mount("/", StaticFiles(directory=str(_DIST_DIR), html=True), name="frontend")
