import React, { useState } from 'react';

const CPP_NODES = {
  ohlcv: {
    title: 'Synchronized OHLCV + Corporate Hygiene (live_market_data.hpp)',
    detail: 'Fetches BIST / XETRA / US / Crypto bars via libcurl + Boost.JSON, aligns split-adjusted closes by exchange calendar day_keys, strips ETFs/warrants in equity universes, and executes fills realistically at Open[t+1].',
  },
  sentiment: {
    title: 'Dual-Stream Sticky Sentiment Memory (news_sentiment.hpp)',
    detail: 'Groups Google News & Yahoo headlines chronologically by UTC day into independent Sticky Positive & Sticky Negative state channels (decay α=0.85), computing sent_count_log, sent_strength, dispersion, and event-shock triggers resistant to neutral weekend fluff.',
  },
  forecast: {
    title: 'QuantForecastEngine C++20 (forecast_engine.hpp)',
    detail: 'Strictly causal trailing features (rev5/10, mom20/60, RSI-rev, OBV flow, CLV, RV spike) combined via James-Stein / Empirical Bayes posterior IC shrinkage, Cornish-Fisher VaR/CVaR cones, and sticky event-shock damping.',
  },
  portfolio: {
    title: 'Cross-Sectional Portfolio & Fake-Alpha Lab (cross_sectional_lab.hpp)',
    detail: 'Ranks clean equities cross-sectionally into Top-N, Long Top-25%, and Market-Neutral L/S portfolios with weight drift and 10 bps turnover costs — audited head-to-head against Vol Top-N, Rand-Vol Monte Carlo, Shuffled-Signal, Timing Spread, and Look-Ahead Causality Mutation Seals.',
  },
};

export default function ArchitectureDiagram() {
  const [selected, setSelected] = useState('forecast');

  return (
    <div className="section-card">
      <div className="section-card-header">
        <h3>ModernQuantSuite C++20 Pipeline &amp; Diagnostics Architecture</h3>
        <span className="section-badge">Click any stage to inspect causality &amp; telemetry</span>
      </div>
      <div className="section-card-body">
        <div className="arch-diagram">
          <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
            <div className="arch-node branch-price" onClick={() => setSelected('ohlcv')}>
              <span className="arch-node-title">📊 Sync OHLCV + Equity Hygiene</span>
              <span className="arch-node-desc">Day-Key Alignment · Open[t+1] · Causal Windows</span>
            </div>
            <div className="arch-node branch-sent" onClick={() => setSelected('sentiment')}>
              <span className="arch-node-title">🧠 Dual-Stream Sticky NLP</span>
              <span className="arch-node-desc">Pos/Neg Memory · Event Shock · sent_strength</span>
            </div>
          </div>

          <div className="arch-arrow">──▶</div>

          <div className="arch-node fusion" onClick={() => setSelected('forecast')} style={{ padding: '18px 22px' }}>
            <span className="arch-node-title">⚡ QuantForecastEngine C++20</span>
            <span className="arch-node-desc">Empirical Bayes IC · Heston Cones · Shock Guard</span>
          </div>

          <div className="arch-arrow">──▶</div>

          <div className="arch-node output" onClick={() => setSelected('portfolio')}>
            <span className="arch-node-title">🔬 CS Portfolio &amp; Fake-Alpha Seal</span>
            <span className="arch-node-desc">CS-IC · Vol Control · MCC · Timing · Ex-Top3</span>
          </div>
        </div>

        <div style={{
          marginTop: 18,
          padding: '14px 18px',
          background: 'var(--bg-secondary)',
          border: '1px solid var(--border-medium)',
          borderRadius: 'var(--radius-md)',
        }}>
          <div style={{ fontWeight: 700, color: 'var(--accent-primary)', marginBottom: 6, fontSize: '0.84rem' }}>
            {CPP_NODES[selected].title}
          </div>
          <div style={{ fontSize: '0.78rem', color: 'var(--text-secondary)', lineHeight: 1.6 }}>
            {CPP_NODES[selected].detail}
          </div>
        </div>
      </div>
    </div>
  );
}
