#!/usr/bin/env python
"""
Start the ModernQuantSuite Visual Research Dashboard (FastAPI + React/Vite).

Usage:
    python dashboard/start.py
    python dashboard/start.py --no-browser
"""

import os
import subprocess
import sys
import time
import webbrowser
from pathlib import Path

DASHBOARD_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = DASHBOARD_DIR.parent
FRONTEND_DIR = DASHBOARD_DIR / "frontend"


def main():
    print("=" * 64)
    print("  ModernQuantSuite C++20 — Visual Research Dashboard Launcher")
    print("=" * 64)

    no_browser = "--no-browser" in sys.argv
    backend_cmd = [
        sys.executable, "-m", "uvicorn",
        "dashboard.backend.main:app",
        "--host", "127.0.0.1",
        "--port", "8000",
    ]
    print(f"[1/2] Starting FastAPI Backend -> http://localhost:8000 (API & reports: {PROJECT_ROOT / 'reports'})")
    backend_proc = subprocess.Popen(backend_cmd, cwd=str(PROJECT_ROOT))

    frontend_proc = None
    has_npm = (FRONTEND_DIR / "node_modules").exists()
    if has_npm:
        npm_bin = "npm.cmd" if os.name == "nt" else "npm"
        print("[2/2] Starting Vite React Dev Server -> http://localhost:5173")
        frontend_proc = subprocess.Popen([npm_bin, "run", "dev"], cwd=str(FRONTEND_DIR))
        ui_url = "http://localhost:5173"
    else:
        print("[2/2] Serving static frontend via FastAPI -> http://localhost:8000")
        ui_url = "http://localhost:8000"

    if not no_browser:
        time.sleep(2.2)
        try:
            webbrowser.open(ui_url)
        except Exception:
            pass

    print("\n" + "=" * 64)
    print(f"  Dashboard Ready: {ui_url}")
    print("  FastAPI Docs   : http://localhost:8000/docs")
    print("  Press Ctrl+C to shut down both servers.")
    print("=" * 64 + "\n")

    try:
        while True:
            time.sleep(1)
            if backend_proc.poll() is not None:
                break
            if frontend_proc is not None and frontend_proc.poll() is not None:
                break
    except KeyboardInterrupt:
        print("\nShutting down dashboard servers...")
    finally:
        for proc in (frontend_proc, backend_proc):
            if proc is not None and proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    proc.kill()
        print("Done.")


if __name__ == "__main__":
    main()
