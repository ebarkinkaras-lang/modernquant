import React, { useState, useEffect, useCallback } from 'react';
import { useApi } from './hooks/useApi';
import Header from './components/Header';
import Sidebar from './components/Sidebar';
import Leaderboard from './components/Leaderboard';
import MetricsCards from './components/MetricsCards';
import StrategyTable from './components/StrategyTable';
import TradeView from './components/TradeView';
import FoldTimeline from './components/FoldTimeline';
import ICTimeSeries from './components/ICTimeSeries';
import RankHeatmap from './components/RankHeatmap';
import RadarComparison from './components/RadarComparison';
import ArchitectureDiagram from './components/ArchitectureDiagram';
import { shortenExpName, formatNum } from './utils/formatters';

const TABS = [
  { id: 'overview', label: 'Overview & Backtest' },
  { id: 'ic', label: 'Cross-Sectional IC' },
  { id: 'stocks', label: 'Per-Stock Predictability' },
  { id: 'trades', label: 'Trade Execution & Candles' },
  { id: 'folds', label: 'Walk-Forward Folds' },
  { id: 'architecture', label: 'C++20 Architecture' },
  { id: 'leaderboard', label: 'Leaderboard' },
];

export default function App() {
  const { data: listData, loading: listLoading } = useApi('/experiments');
  const experiments = listData?.experiments || [];

  const [selectedName, setSelectedName] = useState(null);
  const [comparedName, setComparedName] = useState(null);
  const [activeTab, setActiveTab] = useState('overview');

  useEffect(() => {
    if (experiments.length > 0 && !selectedName) {
      const sorted = [...experiments].sort((a, b) => (b.headline_metric_value ?? 0) - (a.headline_metric_value ?? 0));
      setSelectedName(sorted[0].name);
    }
  }, [experiments, selectedName]);

  const handleSelect = useCallback((name, isShift) => {
    if (isShift && selectedName && name !== selectedName) {
      setComparedName(prev => (prev === name ? null : name));
    } else {
      setSelectedName(name);
      if (comparedName === name) setComparedName(null);
    }
  }, [selectedName, comparedName]);

  const { data: detail, loading: detailLoading } = useApi(
    selectedName ? `/experiment/${encodeURIComponent(selectedName)}` : null
  );
  const { data: comparedDetail } = useApi(
    comparedName ? `/experiment/${encodeURIComponent(comparedName)}` : null
  );

  const renderContent = () => {
    if (detailLoading) {
      return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 16 }}>
          <div className="loading-shimmer" style={{ height: 100 }} />
          <div className="loading-shimmer" style={{ height: 300 }} />
        </div>
      );
    }
    if (!detail) {
      return (
        <div className="empty-state" style={{ height: '60vh' }}>
          <span className="empty-icon">🔬</span>
          <span className="empty-title">No C++20 Research Reports Found Yet</span>
          <span className="empty-hint">
            Run menu <code>[2] -&gt; [7]</code> or <code>.\bin\quant_suite.exe --export-report US</code> to generate reports into <code>reports/</code>.
          </span>
        </div>
      );
    }

    const { task, config, summary, folds, per_stock, backtest, ic_series, strategies } = detail;

    return (
      <div className="fade-in" style={{ display: 'flex', flexDirection: 'column', gap: 'var(--space-lg)' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexWrap: 'wrap', gap: 12 }}>
          <div>
            <h2 style={{ fontSize: '1.2rem', fontWeight: 800, letterSpacing: '-0.02em' }}>
              {shortenExpName(selectedName)}
            </h2>
            <div style={{ display: 'flex', gap: 8, marginTop: 4, alignItems: 'center', flexWrap: 'wrap' }}>
              <span className={`tag ${task}`}>{config?.region || task}</span>
              <span className={`tag ${config?.use_sentiment ? 'sentiment' : 'no-sentiment'}`}>
                {config?.use_sentiment ? '🧠 Sticky Sentiment Active' : '📊 Price/Volume Causal'}
              </span>
              {config?.horizon && (
                <span style={{ fontSize: '0.72rem', color: 'var(--text-muted)', fontFamily: 'var(--font-mono)' }}>
                  H={config.horizon}d · {config?.execution || 'Open[t+1]'} · TC={config?.tc_bps ?? 10}bps
                </span>
              )}
            </div>
          </div>
          {comparedName && (
            <div style={{ fontSize: '0.75rem', color: 'var(--accent-secondary)', fontWeight: 600 }}>
              Comparing with: {shortenExpName(comparedName)}
            </div>
          )}
        </div>

        <MetricsCards summary={summary} config={config} taskType={task} />

        {comparedDetail && comparedDetail.task === task && (
          <RadarComparison
            detail1={detail}
            detail2={comparedDetail}
            name1={selectedName}
            name2={comparedName}
            taskType={task}
          />
        )}

        <div className="section-tabs">
          {TABS.map(tab => (
            <button
              key={tab.id}
              className={`section-tab ${activeTab === tab.id ? 'active' : ''}`}
              onClick={() => setActiveTab(tab.id)}
            >
              {tab.label}
            </button>
          ))}
        </div>

        {activeTab === 'overview' && (
          <>
            {backtest?.length > 0 && <StrategyTable strategies={backtest} />}
            {ic_series?.length > 0 && <ICTimeSeries icSeries={ic_series} name={selectedName} />}
            {folds?.length > 0 && (
              <FoldTimeline
                folds={folds}
                comparedFolds={comparedDetail?.folds}
                taskType={task}
                name={selectedName}
                comparedName={comparedName}
              />
            )}
          </>
        )}

        {activeTab === 'ic' && (
          ic_series?.length > 0 ? (
            <ICTimeSeries icSeries={ic_series} name={selectedName} />
          ) : (
            <div className="empty-state" style={{ padding: 48 }}>
              <span className="empty-title">No Cross-Sectional IC data available</span>
            </div>
          )
        )}

        {activeTab === 'stocks' && (
          per_stock?.length > 0 ? (
            <RankHeatmap perStock={per_stock} taskType={task} />
          ) : (
            <div className="empty-state" style={{ padding: 48 }}>
              <span className="empty-title">No per-stock predictability records available</span>
            </div>
          )
        )}

        {activeTab === 'trades' && (
          strategies?.length > 0 ? (
            <TradeView experimentName={selectedName} strategies={strategies} backtest={backtest} />
          ) : (
            <div className="empty-state" style={{ padding: 48 }}>
              <span className="empty-title">No trade logs exported</span>
            </div>
          )
        )}

        {activeTab === 'folds' && (
          folds?.length > 0 ? (
            <FoldTimeline
              folds={folds}
              comparedFolds={comparedDetail?.folds}
              taskType={task}
              name={selectedName}
              comparedName={comparedName}
            />
          ) : (
            <div className="empty-state" style={{ padding: 48 }}>
              <span className="empty-title">No fold breakdown available</span>
            </div>
          )
        )}

        {activeTab === 'architecture' && <ArchitectureDiagram />}

        {activeTab === 'leaderboard' && (
          <Leaderboard experiments={experiments} onSelect={(n) => { setSelectedName(n); setActiveTab('overview'); }} />
        )}
      </div>
    );
  };

  return (
    <div className="dashboard-layout">
      <Header experimentCount={experiments.length} />
      <Sidebar
        experiments={experiments}
        selected={selectedName}
        compared={comparedName}
        onSelect={handleSelect}
        onClearCompare={() => setComparedName(null)}
        loading={listLoading}
      />
      <main className="main-content">
        {renderContent()}
      </main>
    </div>
  );
}
