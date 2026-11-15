import React, { useMemo } from 'react';
import {
  ComposedChart, Bar, Line, Area, XAxis, YAxis, CartesianGrid,
  Tooltip, ResponsiveContainer, ReferenceLine, Cell
} from 'recharts';
import { formatNum, formatPct } from '../utils/formatters';

export default function ICTimeSeries({ icSeries, name }) {
  const data = useMemo(() => {
    if (!icSeries || icSeries.length === 0) return [];
    const step = icSeries.length > 250 ? 2 : 1;
    return icSeries
      .filter((_, i) => i % step === 0 || i === icSeries.length - 1)
      .map(d => ({
        date: String(d.Date || '').slice(0, 10),
        ic: d.IC,
        rolling20: d.IC_Rolling_20,
        cumSpread: d.Cumulative_Spread,
        nStocks: d.N_Stocks,
      }));
  }, [icSeries]);

  const stats = useMemo(() => {
    if (!icSeries?.length) return {};
    const ics = icSeries.map(d => d.IC).filter(v => v != null);
    const mean = ics.length ? ics.reduce((a, b) => a + b, 0) / ics.length : 0;
    const pos = ics.filter(v => v > 0).length;
    const lastCum = icSeries[icSeries.length - 1]?.Cumulative_Spread ?? 0;
    return { mean, pctPos: ics.length ? (pos / ics.length) * 100 : 0, count: ics.length, lastCum };
  }, [icSeries]);

  const CustomTooltip = ({ active, payload }) => {
    if (!active || !payload?.length) return null;
    const d = payload[0]?.payload;
    return (
      <div style={{
        background: 'var(--bg-secondary)',
        border: '1px solid var(--border-medium)',
        borderRadius: 8,
        padding: '10px 14px',
        fontSize: '0.73rem',
        fontFamily: 'var(--font-mono)',
      }}>
        <div style={{ fontWeight: 700, marginBottom: 4, fontFamily: 'var(--font-sans)' }}>{d.date}</div>
        <div style={{ color: d.ic >= 0 ? 'var(--color-up)' : 'var(--color-down)' }}>Daily IC: {formatNum(d.ic)}</div>
        <div style={{ color: '#6c7bff' }}>20d Rolling IC: {formatNum(d.rolling20)}</div>
        {d.cumSpread != null && (
          <div style={{ color: '#4ecdc4' }}>Cum Q-Spread: {formatPct(d.cumSpread, 1)}</div>
        )}
        <div style={{ color: 'var(--text-muted)', marginTop: 2 }}>{d.nStocks} equities</div>
      </div>
    );
  };

  return (
    <div className="section-card">
      <div className="section-card-header">
        <h3>Cross-Sectional IC &amp; Cumulative Q-Spread Time Series</h3>
        <span className="section-badge">
          {stats.count} days · μ={formatNum(stats.mean)} · {stats.pctPos?.toFixed(1)}% pos · CumSpread={formatPct(stats.lastCum, 1)}
        </span>
      </div>
      <div className="section-card-body">
        <div className="ic-chart-container">
          <ResponsiveContainer width="100%" height="100%">
            <ComposedChart data={data} margin={{ top: 10, right: 30, left: 10, bottom: 0 }}>
              <CartesianGrid strokeDasharray="3 3" stroke="rgba(100,120,220,0.06)" />
              <XAxis dataKey="date" tick={{ fontSize: 10, fill: '#5c6394' }} interval={Math.max(1, Math.floor(data.length / 8))} />
              <YAxis yAxisId="ic" tick={{ fontSize: 10, fill: '#5c6394' }} domain={['auto', 'auto']} />
              <YAxis yAxisId="spread" orientation="right" tick={{ fontSize: 10, fill: '#4ecdc4' }} />
              <Tooltip content={<CustomTooltip />} />
              <ReferenceLine yAxisId="ic" y={0} stroke="rgba(255,255,255,0.15)" />
              <Bar yAxisId="ic" dataKey="ic" name="Daily IC" barSize={3} opacity={0.45}>
                {data.map((entry, idx) => (
                  <Cell key={idx} fill={entry.ic >= 0 ? '#00d395' : '#ff4757'} />
                ))}
              </Bar>
              <Line yAxisId="ic" type="monotone" dataKey="rolling20" name="20d Rolling IC" stroke="#6c7bff" strokeWidth={2} dot={false} />
              <Area
                yAxisId="spread"
                type="monotone"
                dataKey="cumSpread"
                name="Cumulative Q-Spread"
                stroke="#4ecdc4"
                fill="rgba(78, 205, 196, 0.08)"
                strokeWidth={1.5}
              />
            </ComposedChart>
          </ResponsiveContainer>
        </div>
      </div>
    </div>
  );
}
