import React, { useState, useEffect, useRef } from 'react';
import { createChart, CandlestickSeries, HistogramSeries, createSeriesMarkers } from 'lightweight-charts';
import { fetchApi } from '../hooks/useApi';
import { formatNum } from '../utils/formatters';

function TickerRankingList({ rankings, activeTicker, onSelect }) {
  if (!rankings || rankings.length === 0) {
    return (
      <div className="empty-state" style={{ padding: 24 }}>
        <span className="empty-hint">No trade frequency records yet</span>
      </div>
    );
  }
  const maxTrades = rankings[0]?.n_trades || 1;
  return (
    <div className="ticker-ranking-list">
      {rankings.map((r, i) => (
        <div
          key={r.ticker}
          className={`ticker-rank-item ${activeTicker === r.ticker ? 'active' : ''}`}
          onClick={() => onSelect(r.ticker)}
        >
          <span className="ticker-rank-num">{i + 1}</span>
          <span className="ticker-rank-symbol">{r.ticker}</span>
          <div className="ticker-rank-bar">
            <div className="ticker-rank-fill" style={{ width: `${(r.n_trades / maxTrades) * 100}%` }} />
          </div>
          <span className="ticker-rank-count">{r.n_trades}t</span>
        </div>
      ))}
    </div>
  );
}

function CandlestickPanel({ ticker, trades }) {
  const containerRef = useRef(null);
  const chartRef = useRef(null);
  const [priceData, setPriceData] = useState(null);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    if (!ticker) {
      setPriceData(null);
      return;
    }
    setLoading(true);
    fetchApi(`/price/${encodeURIComponent(ticker)}`)
      .then(d => setPriceData(d.candles || []))
      .catch(() => setPriceData([]))
      .finally(() => setLoading(false));
  }, [ticker]);

  useEffect(() => {
    if (!containerRef.current || !priceData || priceData.length === 0) return;
    if (chartRef.current) {
      chartRef.current.remove();
      chartRef.current = null;
    }

    const chart = createChart(containerRef.current, {
      width: containerRef.current.clientWidth,
      height: 400,
      layout: {
        background: { color: '#07080f' },
        textColor: '#9ca3c4',
        fontFamily: 'JetBrains Mono, monospace',
        fontSize: 11,
      },
      grid: {
        vertLines: { color: 'rgba(100,120,220,0.06)' },
        horzLines: { color: 'rgba(100,120,220,0.06)' },
      },
      crosshair: { mode: 0 },
      rightPriceScale: { borderColor: 'rgba(100,120,220,0.12)' },
      timeScale: { borderColor: 'rgba(100,120,220,0.12)', timeVisible: false },
    });

    chartRef.current = chart;

    const candleSeries = chart.addSeries(CandlestickSeries, {
      upColor: '#00d395',
      downColor: '#ff4757',
      borderUpColor: '#00d395',
      borderDownColor: '#ff4757',
      wickUpColor: '#00d395',
      wickDownColor: '#ff4757',
    });
    candleSeries.setData(priceData);

    const volumeSeries = chart.addSeries(HistogramSeries, {
      color: 'rgba(108,123,255,0.15)',
      priceFormat: { type: 'volume' },
      priceScaleId: 'volume',
    });
    chart.priceScale('volume').applyOptions({ scaleMargins: { top: 0.85, bottom: 0 } });
    volumeSeries.setData(
      priceData.map(d => ({
        time: d.time,
        value: d.volume || 0,
        color: d.close >= d.open ? 'rgba(0,211,149,0.2)' : 'rgba(255,71,87,0.2)',
      }))
    );

    if (trades && trades.length > 0) {
      const byDate = {};
      trades.forEach(t => {
        const dt = t.Date || t.date;
        if (dt && !byDate[dt]) byDate[dt] = t;
      });
      const markers = Object.entries(byDate)
        .map(([date, tr]) => {
          const act = String(tr.Action || tr.Direction || '').toLowerCase();
          const isBuy = act.includes('buy');
          return {
            time: date,
            position: isBuy ? 'belowBar' : 'aboveBar',
            color: isBuy ? '#00d395' : '#ff4757',
            shape: isBuy ? 'arrowUp' : 'arrowDown',
            text: isBuy ? 'BUY' : 'SELL',
          };
        })
        .sort((a, b) => a.time.localeCompare(b.time));
      if (markers.length > 0) {
        createSeriesMarkers(candleSeries, markers);
      }
    }

    chart.timeScale().fitContent();

    const handleResize = () => {
      if (containerRef.current && chartRef.current) {
        chartRef.current.applyOptions({ width: containerRef.current.clientWidth });
      }
    };
    window.addEventListener('resize', handleResize);
    return () => {
      window.removeEventListener('resize', handleResize);
      if (chartRef.current) {
        chartRef.current.remove();
        chartRef.current = null;
      }
    };
  }, [priceData, trades]);

  if (!ticker) {
    return (
      <div className="empty-state" style={{ height: 400 }}>
        <span className="empty-icon">📈</span>
        <span className="empty-title">Select a ticker to view OHLCV chart &amp; trade markers</span>
      </div>
    );
  }
  if (loading) return <div className="loading-shimmer" style={{ height: 400, borderRadius: 10 }} />;
  if (!priceData || priceData.length === 0) {
    return (
      <div className="empty-state" style={{ height: 400 }}>
        <span className="empty-icon">⚠️</span>
        <span className="empty-title">No OHLCV data for {ticker}</span>
      </div>
    );
  }
  return <div ref={containerRef} className="candlestick-container" />;
}

export default function TradeView({ experimentName, strategies, backtest }) {
  const [activeStrategy, setActiveStrategy] = useState(null);
  const [rankings, setRankings] = useState([]);
  const [activeTicker, setActiveTicker] = useState(null);
  const [tickerTrades, setTickerTrades] = useState([]);

  useEffect(() => {
    setActiveStrategy(null);
    setActiveTicker(null);
    setRankings([]);
  }, [experimentName]);

  useEffect(() => {
    if (strategies.length > 0 && !activeStrategy) {
      const qf = strategies.find(s => s.includes('QuantForecast') || s.includes('Regression'));
      setActiveStrategy(qf || strategies[0]);
    }
  }, [strategies, backtest, activeStrategy]);

  useEffect(() => {
    if (!activeStrategy || !experimentName) return;
    fetchApi(`/experiment/${encodeURIComponent(experimentName)}/ticker-rankings/${encodeURIComponent(activeStrategy)}`)
      .then(d => {
        setRankings(d.rankings || []);
        if (d.rankings?.length > 0) {
          setActiveTicker(d.rankings[0].ticker);
        }
      })
      .catch(() => setRankings([]));
  }, [activeStrategy, experimentName]);

  useEffect(() => {
    if (!activeStrategy || !experimentName || !activeTicker) {
      setTickerTrades([]);
      return;
    }
    fetchApi(`/experiment/${encodeURIComponent(experimentName)}/trades/${encodeURIComponent(activeStrategy)}`)
      .then(d => {
        setTickerTrades((d.trades || []).filter(t => t.Ticker === activeTicker));
      })
      .catch(() => setTickerTrades([]));
  }, [activeStrategy, experimentName, activeTicker]);

  return (
    <div className="section-card">
      <div className="section-card-header">
        <h3>Per-Stock Trade Execution &amp; OHLCV Candles</h3>
        <span className="section-badge">
          {activeTicker ? `${activeTicker} · ${tickerTrades.length} entries/exits` : 'Select a strategy'}
        </span>
      </div>
      <div className="section-card-body">
        <div className="section-tabs">
          {strategies.map(s => (
            <button
              key={s}
              className={`section-tab ${activeStrategy === s ? 'active' : ''}`}
              onClick={() => { setActiveStrategy(s); setActiveTicker(null); }}
            >
              {s.replace(/_/g, ' ').slice(0, 32)}
            </button>
          ))}
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '280px 1fr', gap: 'var(--space-lg)' }}>
          <div style={{ borderRight: '1px solid var(--border-subtle)', paddingRight: 'var(--space-md)' }}>
            <div style={{ fontSize: '0.68rem', color: 'var(--text-muted)', fontWeight: 600, textTransform: 'uppercase', letterSpacing: '0.06em', marginBottom: 'var(--space-sm)' }}>
              Tickers by Selection Frequency
            </div>
            <TickerRankingList rankings={rankings} activeTicker={activeTicker} onSelect={setActiveTicker} />
          </div>
          <div>
            <CandlestickPanel ticker={activeTicker} trades={tickerTrades} />
            {tickerTrades.length > 0 && (
              <div style={{ marginTop: 12, fontSize: '0.72rem', color: 'var(--text-secondary)', fontFamily: 'var(--font-mono)' }}>
                Latest signal: {tickerTrades[tickerTrades.length - 1].Date} · {tickerTrades[tickerTrades.length - 1].Action} @ {formatNum(tickerTrades[tickerTrades.length - 1].Price, 2)} (Pred: {formatNum((tickerTrades[tickerTrades.length - 1].Predicted_Return ?? 0) * 100, 2)}%)
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
