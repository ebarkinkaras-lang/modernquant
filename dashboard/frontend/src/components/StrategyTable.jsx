import React, { useMemo } from 'react';
import { formatPct, formatNum } from '../utils/formatters';

export default function StrategyTable({ strategies }) {
  const sorted = useMemo(() => {
    return [...strategies].sort((a, b) => (b['Sharpe Ratio'] ?? 0) - (a['Sharpe Ratio'] ?? 0));
  }, [strategies]);

  const isModelRow = (s) =>
    s.Strategy?.includes('QuantForecast') || s.Strategy?.includes('Regression') || s.Strategy?.includes('Model Top');

  const bestModelSharpe = useMemo(() => {
    const modelRows = sorted.filter(isModelRow);
    return modelRows.length > 0 ? Math.max(...modelRows.map(r => r['Sharpe Ratio'] ?? -999)) : 0;
  }, [sorted]);

  const cols = [
    { key: 'Strategy', label: 'Strategy / Fake-Alpha Control', align: 'left' },
    { key: 'Total Return', label: 'Total Return', fmt: 'pct' },
    { key: 'Annualized Return', label: 'Ann. Return', fmt: 'pct' },
    { key: 'Sharpe Ratio', label: 'Sharpe', fmt: 'num2' },
    { key: 'Sortino Ratio', label: 'Sortino', fmt: 'num2' },
    { key: 'Max Drawdown', label: 'Max DD', fmt: 'pct' },
    { key: 'Daily Win Rate', label: 'Win Rate', fmt: 'pct' },
    { key: 'Total Turnover', label: 'Turnover', fmt: 'num2' },
  ];

  const format = (val, fmt) => {
    if (val == null) return '—';
    if (fmt === 'pct') return formatPct(val);
    if (fmt === 'num2') return formatNum(val, 2);
    return String(val);
  };

  return (
    <div className="section-card">
      <div className="section-card-header">
        <h3>Cross-Sectional Portfolio vs Fake-Alpha &amp; Benchmark Controls</h3>
        <span className="section-badge">{strategies.length} portfolios · drifted weights &amp; 10 bps cost</span>
      </div>
      <div className="section-card-body" style={{ overflowX: 'auto' }}>
        <table className="strategy-table">
          <thead>
            <tr>
              {cols.map(c => (
                <th key={c.key} style={c.align === 'left' ? { textAlign: 'left' } : {}}>{c.label}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {sorted.map((s, i) => {
              const isModel = isModelRow(s);
              const isBest = isModel && s['Sharpe Ratio'] === bestModelSharpe;
              return (
                <tr key={i} className={isBest ? 'best-strategy' : ''}>
                  {cols.map(c => {
                    const val = s[c.key];
                    const style = {};
                    if (c.key === 'Total Return' && val != null) {
                      style.color = val >= 0 ? 'var(--color-up)' : 'var(--color-down)';
                    }
                    if (c.key === 'Max Drawdown' && val != null) {
                      style.color = 'var(--color-down)';
                    }
                    if (!isModel && c.key !== 'Strategy') {
                      style.opacity = 0.72;
                    }
                    return (
                      <td key={c.key} style={style}>
                        {c.key === 'Strategy' ? val : format(val, c.fmt)}
                      </td>
                    );
                  })}
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
}
