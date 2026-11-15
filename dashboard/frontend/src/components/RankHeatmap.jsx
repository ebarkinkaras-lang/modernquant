import React, { useState, useMemo } from 'react';
import { formatNum, formatPct } from '../utils/formatters';

function getHeatColor(value, min, max) {
  if (value == null || isNaN(value)) return 'transparent';
  const norm = max === min ? 0.5 : (value - min) / (max - min);
  const clamped = Math.max(0, Math.min(1, norm));
  if (clamped >= 0.5) {
    const t = (clamped - 0.5) * 2;
    return `rgba(0, 211, 149, ${(0.08 + t * 0.28).toFixed(2)})`;
  }
  const t = (0.5 - clamped) * 2;
  return `rgba(255, 71, 87, ${(0.08 + t * 0.28).toFixed(2)})`;
}

export default function RankHeatmap({ perStock, taskType }) {
  const isClassification = taskType === 'classification';
  const [sortCol, setSortCol] = useState(isClassification ? 'accuracy' : 'ic');
  const [sortAsc, setSortAsc] = useState(false);

  const metricCols = isClassification
    ? ['accuracy', 'lift', 'mcc', 'f1_macro']
    : ['ic', 'dir_hit_rate_ex_neutral', 'lift', 'mcc', 'timing_spread_pct', 'topn_contrib_pct'];

  const colLabels = {
    ic: 'Time-IC',
    dir_hit_rate_ex_neutral: 'Dir Hit%',
    lift: 'Lift',
    mcc: 'MCC',
    f1_macro: 'Macro F1',
    accuracy: 'Accuracy',
    timing_spread_pct: 'Timing% (Ann)',
    topn_contrib_pct: 'Top-N Contrib%',
  };

  const ranges = useMemo(() => {
    const r = {};
    metricCols.forEach(c => {
      const vals = perStock.map(s => s[c]).filter(v => v != null && !isNaN(v));
      r[c] = {
        min: vals.length ? Math.min(...vals) : 0,
        max: vals.length ? Math.max(...vals) : 1,
      };
    });
    return r;
  }, [perStock]);

  const sorted = useMemo(() => {
    return [...perStock].sort((a, b) => {
      const va = a[sortCol] ?? 0;
      const vb = b[sortCol] ?? 0;
      return sortAsc ? va - vb : vb - va;
    });
  }, [perStock, sortCol, sortAsc]);

  const handleSort = (col) => {
    if (sortCol === col) setSortAsc(!sortAsc);
    else { setSortCol(col); setSortAsc(false); }
  };

  return (
    <div className="section-card">
      <div className="section-card-header">
        <h3>Per-Stock Predictability, Timing Spread &amp; Portfolio Attribution</h3>
        <span className="section-badge">{perStock.length} equities · click columns to sort</span>
      </div>
      <div className="section-card-body" style={{ overflowX: 'auto', maxHeight: 460 }}>
        <table className="heatmap-table">
          <thead>
            <tr>
              <th style={{ textAlign: 'left', cursor: 'pointer' }} onClick={() => handleSort('ticker')}>Ticker</th>
              <th>N</th>
              <th>Picks</th>
              {metricCols.map(c => (
                <th key={c} style={{ cursor: 'pointer' }} onClick={() => handleSort(c)}>
                  {colLabels[c] || c} {sortCol === c ? (sortAsc ? '↑' : '↓') : ''}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {sorted.map((s) => (
              <tr key={s.ticker}>
                <td className="ticker-cell">{s.ticker}</td>
                <td style={{ color: 'var(--text-muted)' }}>{s.n_samples ?? '—'}</td>
                <td style={{ color: 'var(--text-secondary)' }}>{s.topn_picks ?? '—'}</td>
                {metricCols.map(c => {
                  const val = s[c];
                  const isPct = c === 'dir_hit_rate_ex_neutral' || c === 'lift' || c === 'accuracy';
                  const display = (c === 'timing_spread_pct' || c === 'topn_contrib_pct')
                    ? `${val >= 0 ? '+' : ''}${formatNum(val, 1)}%`
                    : (isPct ? formatPct(val, 1) : formatNum(val, 3));
                  return (
                    <td key={c} style={{ background: getHeatColor(val, ranges[c]?.min ?? 0, ranges[c]?.max ?? 1) }}>
                      {display}
                    </td>
                  );
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
