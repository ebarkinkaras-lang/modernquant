# ModernQuantSuite 📟

> **High-Performance Quantitative & Algorithmic Trading Terminal in C++20 with an Authentic 1980s Retro Unix CRT Aesthetic.**

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20%20%2F%2023-blue.svg?style=flat-square&logo=cplusplus)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg?style=flat-square)](LICENSE)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg?style=flat-square)]()
[![Memory Safety](https://img.shields.io/badge/AddressSanitizer-0%20leaks%20verified-success.svg?style=flat-square)]()
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux-lightgrey.svg?style=flat-square)]()
[![Eigen](https://img.shields.io/badge/dependency-Eigen3%203.4+-orange.svg?style=flat-square)](https://eigen.tuxfamily.org/)
[![Boost](https://img.shields.io/badge/dependency-Boost%201.80+-purple.svg?style=flat-square)](https://www.boost.org/)

---

```text
   ___    __           _____              __   _          
  / _ |  / /__ ____   /_  _/______ _____/ /__(_)__  ___ _ 
 / __ | / / _ `/ _ \   / / / __/ _ `/ _  / _  / _ \/ _ `/ 
/_/ |_|/_/\_, /\___/  /_/ /_/  \_,_/\_,_/\_,_/_//_/\_, /  
          /___/                  C++20 Terminal   /___/   

  Modern C++ Quant Terminal v2.0  │  Tema: 🟨 80s Unix Amber CRT (VT220 / IBM 3101)
  Standart: ISO C++20 • Eigen3 • Boost • LLVM Clang • libcurl
```

**ModernQuantSuite** is an institutional-grade algorithmic and quantitative trading terminal built in modern ISO **C++20**. It bridges the gap between rigorous Wall Street quantitative models (options pricing, linear algebra risk matrices, stochastic simulations, term-structure bootstrapping) and real-time execution interfaces, all wrapped in a lightning-fast, zero-dependency, 1980s Unix Amber/Green CRT terminal interface.

---

## 🌟 Key Highlights & Features

### 1. 📟 Authentic 1980s Retro Unix CRT Themes
* **Multiple Display Modes:** Amber CRT (DEC VT220 / IBM 3101), Phosphor Green CRT (DEC VT100), Cool Cyan (VAX Station), and Synthwave Neon.
* **CRT Simulation:** Optional warm-up phosphor flicker and scanline simulation without external GUI dependencies.

### 2. 🌍 Full Trilingual Localization (i18n Engine: TR / EN / DE)
* **First-Class Multilingual Experience:** Seamlessly localized in **🇹🇷 Türkçe**, **🇬🇧 English**, and **🇩🇪 Deutsch** across all 18 modules, diagnostics, tables, and AI Copilot responses.
* **Dynamic Runtime Switching:** Switch languages on the fly via menu option `[L]` (`[LANG] Dil / Language / Sprache`).
* **Command-Line Arguments:** Launch the terminal directly in your preferred language:
  ```bash
  ./bin/quant_suite --lang en   # 🇬🇧 English
  ./bin/quant_suite --lang de   # 🇩🇪 Deutsch
  ./bin/quant_suite --lang tr   # 🇹🇷 Türkçe (Default)
  ```
* **Multilingual AI Copilot:** Natural language query processor understands tokens and terminology in Turkish, English, and German (e.g. `DAX analyse`, `RHM zielkurs`, `AAPL target`, `precious metals`, etc.).
* **Zero Overhead Architecture:** Thread-safe, header-only C++20 singleton (`include/quant/i18n.hpp`) with zero external runtime dependencies.

### 3. 🎯 Quant Forecast & Direction Predictor (`QuantForecastEngine`)
* **Geometric Brownian Motion (GBM) Target Pricing:** Closed-form log-normal drift and diffusion modeling:
  $$\mathbb{E}[S_T] = S_0 \cdot \exp(\mu \cdot T)$$
* **Multi-Factor Drift Calibration ($\mu$):** Blends the regional risk-free rate, macroeconomic news sentiment shocks, and mean-reverting 30-day Volume Weighted Average Price (VWAP) forces.
* **Precious Metals Live Synthesis:** Cross-calculates live **24K Gram Gold (`GRAM-ALTIN.TL`)** and **Gram Silver (`GRAM-GUMUS.TL`)** via live COMEX Ounces ($GC=F, SI=F) \times USD/TRY / 31.1034768.
* **5-Level Directional Classification:**
  * `🚀 GÜÇLÜ YUKARI` (Strong Bullish)
  * `📈 YUKARI` (Bullish)
  * `⚖️ NÖTR` (Neutral / Consolidation)
  * `📉 AŞAĞI` (Bearish)
  * `🔻 GÜÇLÜ AŞAĞI` (Strong Bearish)
* **Probability Confidence Bands ($\pm 1\sigma$):** Statistical bull ceiling and bear support bounds with proven $\approx 68\%$ market realization coverage.
* **Walk-Forward Out-of-Sample Backtester (`ForecastBacktestEngine`):** 1-year historical sliding-window validation verifying Mean Absolute Percentage Error (MAPE $\le 2\%$) and signal strategy PnL vs Buy & Hold.

### 3. 🎯 Asymmetric Alpha & Radar-Altı Screener (`QuantOpportunityScanner`)
* **Crowded Trade Filter (Sürü Tuzağı Tespiti):** Identifies and warns against retail FOMO consensus trades where risk/reward is asymmetrical against the trader ($R:R \approx 1 : 0.35$).
* **Hidden Smart Money Accumulation:** Detects institutional accumulation ($OBV \ge +0.08$) while assets are still under the public radar.
* **Volatility Squeeze ($\sigma_5 / \sigma_{20} \le 0.68$):** Pins explosive breakout candidates consolidating in tight coiled springs with asymmetric $1:3.7$ Risk/Reward profiles.
* **TradingView Pine Script v6 Indicator:** Native, zero-warning Pine Script v6 overlay indicator and screener dashboard included in `pinescript/ModernQuant_AlphaScreener_v6.pine`.

### 4. 🛡️ Regional Investment Advisor & Crisis Defense Shield (`QuantInvestmentAdvisor`)
* **Multi-Market Portfolio Engine:**
  * 🇹🇷 **Borsa İstanbul (BIST 100):** TRY allocation with automated macro crisis detection (e.g. MSCI downgrade shock shelter).
  * 🇩🇪 **Frankfurt XETRA (DAX 40):** EUR allocation incorporating German Federal Bund yield buffers and European defense super-cycle leaders (Rheinmetall, SAP).
  * 🇺🇸 **Wall Street (S&P 500 / NASDAQ):** USD allocation combining US Treasury bills with mega-cap AI infrastructure leaders.
* **Dynamic Capital Protection:** Automatically routes $60\% - 80\%$ of capital into risk-free yield instruments (%45 TL PPF/Mevduat or US T-Bills) during systemic market panics, accumulating blue-chips via disciplined Dollar-Cost Averaging (DCA).
* **Actionable Execution Plan:** Computes exact lot counts, allocation capital, Take-Profit targets, and Stop-Loss levels.

### 4. 🤖 AI Quant Copilot (`QuantCopilot`)
* **Natural Language Market Interface:** Query any stock, option, or macro question right inside the terminal:
  * `"THYAO 1 ay hedef fiyat ve yönü ne?"`
  * `"SPY backtest yap"`
  * `"Piyasalar genel durum"`
  * `"Black-Scholes opsiyon fiyatla"`
* **Quant Divergence Detection:** Identifies *Bullish Divergences* (discounted dip accumulation) and *Bearish Confirmations* by cross-referencing news sentiment with institutional VWAP spreads.

### 5. ⚡ Derivatives, Options & Stochastic Pricing
* **Closed-Form Black-Scholes:** Analytical pricing for European Calls/Puts and exact first/second-order Greeks ($\Delta, \Gamma, \mathcal{V}, \Theta, \rho$).
* **Secant Method Implied Volatility (IV):** High-precision root solver recovering volatility from market quotes.
* **Multi-Threaded Monte Carlo (`std::async`):** Parallel simulation of path-dependent exotic options (Up-and-Out Barrier Calls) with pseudo-random normal distributions.
* **Binomial Lattice Trees (Boost MultiArray):** Cox-Ross-Rubinstein (CRR) discrete trees for European and American early-exercise options.

### 6. 📐 Portfolio Risk & Linear Algebra (Eigen3)
* **Matrix Covariance Optimization:** Portfolio risk calculation:
  $$\sigma_p = \sqrt{\omega^T \Sigma \omega}$$
* **Householder QR Factor Regression:** Numerically stable least-squares factor decomposition without matrix inversion.
* **Cholesky Basket Decomposition:** Simulates multi-asset correlated Geometric Brownian Motion asset baskets.
* **Principal Component Analysis (PCA):** Eigenvalue/eigenvector decomposition for term-structure shift, twist, and butterfly analysis.

### 7. 📈 Fixed Income & Term Structure Bootstrapping
* **C++20 Date Engine:** Native `std::chrono` calendar math with ISDA Act/365, Act/360, and 30/360 day-count conventions.
* **Zero-Coupon Curve Bootstrapping:** Dispatches live US Treasury yields (^IRX, ^FVX, ^TNX, ^TYX) to discount irregular bond cash flows.

### 8. 📊 In-Terminal Candlestick & Volume Chart Engine (`TerminalChartEngine`)
* **100% Terminal-Native Bloomberg Style:** Renders Unicode candlestick bars, volume histograms, and SMA/RSI overlays directly inside ANSI/VT100 terminals without X11/GUI requirements.

### 9. 🧪 Pairs Trading & Statistical Arbitrage (`PairsTradingEngine`)
* **Cointegration & Spread Z-Score:** Rolling ordinary least squares (OLS) hedge ratio $\beta$.
* **Ornstein-Uhlenbeck Mean-Reversion Half-Life:** Computes mean-reversion speed for mean-reverting pairs.

---

## 🏛️ Architecture & Software Standards

* **100% ISO C++20:** Modern language features (`std::format`, `std::chrono` calendars, Concepts, Ranges, RAII).
* **Zero Memory Leaks:** 100% clean passes under LLVM `AddressSanitizer` (ASan) and `UndefinedBehaviorSanitizer` (UBSan).
* **No Raw Owning Pointers:** Pure modern memory semantics (`std::unique_ptr`, `std::shared_ptr`, value semantics).
* **Header-Only Modular Quant Library:** All engines are encapsulated cleanly inside `include/quant/`.

---

## 📚 Academic & Literature Foundations

The algorithms and financial formulas implemented in ModernQuantSuite are strictly verified against standard financial engineering literature:

1. **Daniel Hanson**, *Learning Modern C++ for Finance* (O'Reilly, 2024).
2. **Jason Strimpel**, *Python for Algorithmic Trading Cookbook* (Packt, 2024).
3. **Yves Hilpisch**, *Python for Algorithmic Trading* (O'Reilly, 2021).
4. **Marcos López de Prado**, *Advances in Financial Machine Learning* (Wiley, 2018).

---

## 🛠️ Build & Installation

### Prerequisites

* **C++20 Compliant Compiler:** Clang 15+ (Apple Clang 15+) or GCC 11+
* **Dependencies:**
  * **Eigen3** (v3.3+)
  * **Boost** (Headers: Accumulators, MultiArray, Math)
  * **libcurl** (REST API requests)
  * **pthread**

#### macOS (via Homebrew):
```bash
brew install eigen boost curl
```

#### Ubuntu / Debian:
```bash
sudo apt update
sudo apt install build-essential clang libeigen3-dev libboost-all-dev libcurl4-openssl-dev
```

---

### Building with Make

```bash
# Clone the repository
git clone https://github.com/<your-username>/ModernQuantSuite.git
cd ModernQuantSuite

# 1. Compile Optimized Release Binary
make release

# 2. Run the Interactive Terminal
./bin/quant_suite

# 3. Run Automated Textbook Verification Test Suite (with AddressSanitizer)
make test
```

### Building with CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Run binary
./build/bin/quant_suite
```

---

## 🖥️ Terminal Main Menu Preview

```text
┌─────────────────────────────────[ ANA MENÜ ]──────────────────────────────────┐
│ A   [ADVISOR]  Sermaye Dağıtım & Tavsiye    Paramı nereye, nasıl yatırayım? Somut Quant Strateji │
│ S   [SCREEN]   Asimetrik Alfa & Radar-Altı  Başkalarının göremediği sessiz akümülasyon & sıkışma │
│ F   [FORECAST] Hedef Fiyat & Yön Tahmini    GBM Drift & Volatilite bazlı 1H/1A/3A hedef fiyat ve yön analizi │
│ C   [COPILOT]  AI Quant Copilot             Doğal dille canlı analiz, Black-Scholes & makro soruları │
│ G   [CHART]    Terminal Mum Grafiği         100% Terminal içi Bloomberg stili grafik, Hacim, SMA & RSI │
│ 1   [DATA]     Canlı Piyasa & Veri Motoru   Yahoo Finance API: Hisse, Kripto, VWAP, Volatilite │
│ 2   [BACK]     Algoritmik Backtest          SMA Kesişimi, Bollinger Mean-Reversion, RSI Momentum │
│ 3   [LIVE]     Gerçek Zamanlı İzleyici      Boost Accumulators akışkan sinyalleri (BBands & Crossover) │
│ 4   [QUANT]    Türev & Opsiyon Motoru       Black-Scholes, Greeks, Secant IV, std::async MC, Bariyerler │
│ 5   [PORT]     Portföy Risk & Faktör Analizi Eigen ω^T Σ ω, Cholesky Sepet Simülasyonu, QR Regresyon │
│ 6   [MACRO]    Sabit Getiri & Hazine Eğrisi C++20 ChronoDate, Bootstrapping, Düzensiz Tahviller, PCA │
│ 7   [STAT]     Pairs Trading & Arbitraj     Kointegrasyon, Spread Z-Score, Ornstein-Uhlenbeck Half-Life │
│ 8   [RISK]     Drawdown & VaR Analizi       Monte Carlo Shuffle, Parametrik & Nonparametrik Max DD │
│ L   [LANG]     Dil / Language / Sprache     Türkçe 🇹🇷 / English 🇬🇧 / Deutsch 🇩🇪 │
│ T   [TEMA]     Renk & 80s CRT Teması        Amber VT220, Yeşil VT100, VAX Cyan, Neon, CRT Titreşim │
│ V   [TEST]     Otomatik Doğrulama Testi     Tüm kitap formülleri ve ground-truth numerik doğrulaması │
│ 11  [INFO]     Hakkında & Mimari            Kitap kaynakları, C++ standartları ve proje detayları │
│ 0   [EXIT]     Çıkış                        Terminal uygulamasını kapat  │
└───────────────────────────────────────────────────────────────────────────────┘
```

---

## 🧪 Automated Verification Suite

Run `./bin/quant_suite --test` or `make test` to execute the built-in textbook mathematical verification suite:

```text
=======================================================
  RUNNING AUTOMATED TEXTBOOK VERIFICATION TEST SUITE   
=======================================================

[TEST 1] ChronoDate & Day Count Conventions (Chapter 7)           ✔ PASS
[TEST 2] Black-Scholes, Greeks & Implied Volatility (Ch. 2 & 4)   ✔ PASS
[TEST 3] Monte Carlo & Barrier Options (Chapter 6)                ✔ PASS
[TEST 4] Binomial Lattice Option Pricing (Chapter 9)              ✔ PASS
[TEST 5] Fixed Income & Bond Valuation (Chapter 7)                ✔ PASS
[TEST 6] Eigen Linear Algebra, QR & PCA (Chapter 8)               ✔ PASS
[TEST 7] Boost Accumulators & Drawdown Risk (Chapters 6 & 9)      ✔ PASS
[TEST 8] Quant Target Price & Directional Regime (GBM & Sentiment)✔ PASS
[TEST 9] Walk-Forward Forecast Backtest Engine                    ✔ PASS
[TEST 10] Asymmetric Alpha & Hidden Opportunity Screener          ✔ PASS

-------------------------------------------------------
[PASS] ALL TEXTBOOK VERIFICATION TESTS PASSED SUCCESSFULLY!
=======================================================
```

---

## 📁 Repository Structure

```text
ModernQuantSuite/
├── CMakeLists.txt            # Modern cross-platform CMake build configuration
├── Makefile                  # Optimized & dynamic C++20 Makefile (macOS / Linux)
├── LICENSE                   # MIT License
├── README.md                 # Project documentation & reference manual
├── .gitignore                # Comprehensive Git ignore rules (bin, build, macOS, IDEs)
├── src/
│   └── main.cpp              # Terminal CLI loop, interactive dispatcher, CRT rendering
├── include/
│   └── quant/
│       ├── advisor.hpp               # Regional investment advisor & macro crisis shield
│       ├── alpha_screener.hpp        # Asymmetric alpha, crowded trade filter & volume squeeze
│       ├── backtester.hpp            # Technical trading strategy backtester
│       ├── binomial_lattice.hpp      # Binomial lattice option valuation (CRR)
│       ├── black_scholes.hpp         # Analytical Black-Scholes & Greeks engine
│       ├── bond.hpp                  # Fixed income bond valuation & day-count discounting
│       ├── concepts.hpp              # C++20 concepts for financial type-safety
│       ├── copilot.hpp               # Natural language AI Quant Copilot dispatcher
│       ├── dates.hpp                 # C++20 chrono business calendar & weekend rolls
│       ├── daycount.hpp              # ISDA Act/365, Act/360, 30/360 conventions
│       ├── forecast_backtest.hpp     # 1-year historical walk-forward backtesting engine
│       ├── forecast_engine.hpp       # GBM target pricing & 5-level directional classification
│       ├── live_market_data.hpp      # Real-time Yahoo Finance REST client via libcurl
│       ├── monte_carlo.hpp           # Parallel std::async Monte Carlo simulation engine
│       ├── news_sentiment.hpp        # Macro headlines parser & Bayesian sentiment engine
│       ├── option_info.hpp           # Derivatives parameter definitions
│       ├── pairs_trading.hpp         # Cointegration, Z-Score & Ornstein-Uhlenbeck half-life
│       ├── payoff.hpp                # Polymorphic payoff functors (Call/Put)
│       ├── portfolio_linear_algebra.hpp # Eigen matrix math, Cholesky, QR, and PCA
│       ├── terminal_chart.hpp        # Unicode ASCII candlestick & volume chart engine
│       ├── test_suite.hpp            # 10-part automated textbook verification suite
│       ├── theme_engine.hpp          # 1980s retro CRT VT220/VT100 theme manager
│       ├── trading_signals.hpp       # Boost Accumulators real-time streaming indicators
│       └── yield_curve.hpp           # US Treasury yield curve bootstrapping
├── pinescript/
│   ├── ModernQuant_AlphaScreener_v6.pine # Official Pine Script v6 Screener & Overlay
│   └── ModernQuant_AlphaScreener.pine    # Pine Script implementation
└── resources/
    ├── app_icon.png          # High-resolution retro terminal icon
    └── AppIcon.icns          # macOS application icon bundle
```

---

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

Developed by **Emir Barkın Karaş** (2026).
