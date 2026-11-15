"""
Experiment report loader for the ModernQuantSuite Visual Research Dashboard.

Parses C++ exported reports in `modernquant/reports/<experiment_name>/`:
- config.json
- summary.csv
- per_fold_results.csv
- per_stock_results.csv
- cross_sectional_ic.csv
- backtest_results.csv
- *_trades.csv & *_holdings.csv
"""

import csv
import json
import math
from pathlib import Path
from typing import Any, Dict, List, Optional

PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
REPORTS_DIR = PROJECT_ROOT / "reports"


def _clean_val(v: Any) -> Any:
    if v is None:
        return None
    if isinstance(v, (int, float)):
        if math.isnan(v) or math.isinf(v):
            return None
        return v
    if isinstance(v, str):
        s = v.strip()
        if s == "" or s.lower() in {"nan", "inf", "-inf", "null", "none"}:
            return None
        if s.lower() == "true":
            return True
        if s.lower() == "false":
            return False
        try:
            num = float(s)
            if math.isnan(num) or math.isinf(num):
                return None
            return int(s) if s.isdigit() or (s.startswith("-") and s[1:].isdigit()) else num
        except ValueError:
            return s
    return v


def _parse_csv_rows(path: Path) -> List[Dict[str, Any]]:
    if not path.exists():
        return []
    rows: List[Dict[str, Any]] = []
    with open(path, "r", encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.DictReader(f)
        for raw in reader:
            cleaned = {k.strip(): _clean_val(v) for k, v in raw.items() if k is not None}
            rows.append(cleaned)
    return rows


def _parse_config(exp_dir: Path) -> Dict[str, Any]:
    cfg_path = exp_dir / "config.json"
    if not cfg_path.exists():
        return {}
    try:
        with open(cfg_path, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except Exception:
        return {}


def _detect_task(name: str, summary: Dict[str, Any], cfg: Dict[str, Any]) -> str:
    task = str(cfg.get("task") or summary.get("task") or "").lower().strip()
    if task in {"classification", "regression"}:
        return task
    if "wf_agg_ic" in summary or "cross_sectional_ic" in summary:
        return "regression"
    return "classification" if "clf" in name.lower() else "regression"


def _list_strategy_files(exp_dir: Path, suffix: str) -> List[str]:
    names = []
    for p in sorted(exp_dir.glob(f"*{suffix}")):
        names.append(p.name[: -len(suffix)])
    return names


def list_experiments(reports_dir: Path = None) -> List[Dict[str, Any]]:
    rdir = reports_dir or REPORTS_DIR
    if not rdir.exists():
        return []

    experiments: List[Dict[str, Any]] = []
    for d in sorted(rdir.iterdir()):
        if not d.is_dir() or d.name.startswith(".") or d.name in {"ohlcv", "__pycache__"}:
            continue

        summary_rows = _parse_csv_rows(d / "summary.csv")
        if not summary_rows and not (d / "config.json").exists():
            continue

        summary = summary_rows[0] if summary_rows else {}
        cfg = _parse_config(d)
        task = _detect_task(d.name, summary, cfg)
        use_sentiment = bool(cfg.get("use_sentiment", summary.get("use_sentiment", True)))
        folds = _parse_csv_rows(d / "per_fold_results.csv")
        backtest_rows = _parse_csv_rows(d / "backtest_results.csv")

        if task == "regression":
            headline_key = "cross_sectional_ic" if summary.get("cross_sectional_ic") is not None else "wf_agg_ic"
        else:
            headline_key = "wf_agg_accuracy"
        headline_val = _clean_val(summary.get(headline_key, 0.0))

        model_type = str(cfg.get("model_type") or "QuantForecastEngine C++20")
        region = str(cfg.get("region") or summary.get("region") or "LIVE")

        experiments.append({
            "name": d.name,
            "task": task,
            "region": region,
            "use_sentiment": use_sentiment,
            "architecture": model_type.replace("_", " "),
            "n_params": _clean_val(cfg.get("n_params", 18)),
            "n_folds": len(folds),
            "has_folds": len(folds) > 0,
            "has_backtest": len(backtest_rows) > 0,
            "has_ic": (d / "cross_sectional_ic.csv").exists(),
            "has_per_stock": (d / "per_stock_results.csv").exists(),
            "headline_metric_name": headline_key,
            "headline_metric_value": headline_val if headline_val is not None else 0.0,
            "summary": summary,
            "config": cfg,
        })

    return experiments


def get_experiment_detail(name: str, reports_dir: Path = None) -> Optional[Dict[str, Any]]:
    rdir = reports_dir or REPORTS_DIR
    exp_dir = rdir / name
    if not exp_dir.exists() or not exp_dir.is_dir():
        return None

    cfg = _parse_config(exp_dir)
    summary_rows = _parse_csv_rows(exp_dir / "summary.csv")
    summary = summary_rows[0] if summary_rows else {}
    task = _detect_task(name, summary, cfg)

    folds = _parse_csv_rows(exp_dir / "per_fold_results.csv")
    per_stock = _parse_csv_rows(exp_dir / "per_stock_results.csv")
    backtest = _parse_csv_rows(exp_dir / "backtest_results.csv")
    ic_series = _parse_csv_rows(exp_dir / "cross_sectional_ic.csv")
    strategies = _list_strategy_files(exp_dir, "_trades.csv")

    return {
        "name": name,
        "task": task,
        "config": cfg,
        "summary": summary,
        "folds": folds,
        "per_stock": per_stock,
        "backtest": backtest,
        "ic_series": ic_series,
        "strategies": strategies,
    }


def get_cross_sectional_ic(name: str, reports_dir: Path = None) -> List[Dict[str, Any]]:
    rdir = reports_dir or REPORTS_DIR
    return _parse_csv_rows(rdir / name / "cross_sectional_ic.csv")


def get_trades(name: str, strategy: str, reports_dir: Path = None) -> List[Dict[str, Any]]:
    rdir = reports_dir or REPORTS_DIR
    return _parse_csv_rows(rdir / name / f"{strategy}_trades.csv")


def get_holdings(name: str, strategy: str, reports_dir: Path = None) -> List[Dict[str, Any]]:
    rdir = reports_dir or REPORTS_DIR
    return _parse_csv_rows(rdir / name / f"{strategy}_holdings.csv")


def get_ticker_rankings(name: str, strategy: str, reports_dir: Path = None) -> List[Dict[str, Any]]:
    rdir = reports_dir or REPORTS_DIR
    exp_dir = rdir / name
    trades = _parse_csv_rows(exp_dir / f"{strategy}_trades.csv")
    if not trades:
        return []

    stats: Dict[str, Dict[str, Any]] = {}
    for tr in trades:
        tkr = str(tr.get("Ticker") or tr.get("ticker") or "")
        if not tkr:
            continue
        rec = stats.setdefault(tkr, {"ticker": tkr, "n_trades": 0, "buys": 0, "sells": 0, "avg_pnl": 0.0, "_pnls": []})
        rec["n_trades"] += 1
        act = str(tr.get("Action") or tr.get("Direction") or "").upper()
        if "BUY" in act:
            rec["buys"] += 1
        elif "SELL" in act:
            rec["sells"] += 1
        pnl = _clean_val(tr.get("PnL_Pct"))
        if isinstance(pnl, (int, float)):
            rec["_pnls"].append(float(pnl))

    out = []
    for rec in stats.values():
        pnls = rec.pop("_pnls", [])
        rec["avg_pnl"] = round(sum(pnls) / len(pnls), 3) if pnls else 0.0
        out.append(rec)

    out.sort(key=lambda r: (r["n_trades"], r["avg_pnl"]), reverse=True)
    return out
