import React from 'react';
import { formatPct, formatNum, formatInt } from '../utils/formatters';

function MetricCard({ label, value, sub, badge, badgeType }) {
  return (
    <div className="metric-card">
      <div className="metric-label">
        {label}
        {badge && (
          <span
            className={`tag ${badgeType || 'classification'}`}
            style={{ marginLeft: 8, fontSize: '0.58rem', padding: '1px 6px' }}
          >
            {badge}
          </span>
        )}
      </div>
      <div className="metric-value">{value}</div>
      {sub && <div className="metric-sub">{sub}</div>}
    </div>
  );
}

export default function MetricsCards({ summary, config, taskType }) {
  if (!summary) return null;

  const isClassification = taskType === 'classification';

  const cards = isClassification
    ? [
        {
          label: 'Accuracy (3-Class)',
          value: formatPct(summary.wf_agg_accuracy),
          sub: `Zero-Rule: ${formatPct((summary.wf_agg_accuracy ?? 0) - (summary.wf_agg_lift ?? 0))}`,
        },
        {
          label: 'Lift vs Zero-Rule',
          value: `${(summary.wf_agg_lift ?? 0) >= 0 ? '+' : ''}${formatPct(summary.wf_agg_lift)}`,
          sub: summary.wf_agg_lift > 0 ? 'Beats majority class' : 'Below baseline',
        },
        {
          label: 'Matthews Corr (MCC)',
          value: formatNum(summary.wf_agg_mcc),
          sub: `Macro F1: ${formatNum(summary.wf_agg_f1_macro)}`,
        },
      ]
    : [
        {
          label: 'Mean Cross-Sectional IC',
          value: formatNum(summary.cross_sectional_ic),
          sub: `IC-IR: ${formatNum(summary.ic_ir, 3)} · +IC Days: ${formatNum(summary.pct_positive_ic, 1)}%`,
          badge: (summary.cross_sectional_ic ?? 0) > 0.01 ? 'POSITIVE' : 'WEAK',
          badgeType: (summary.cross_sectional_ic ?? 0) > 0.01 ? 'sentiment' : 'no-sentiment',
        },
        {
          label: 'Directional Hit (Ex-Neutral)',
          value: formatPct(summary.wf_agg_directional_accuracy),
          sub: `3-Cls Acc: ${formatPct(summary.wf_agg_accuracy)} (Lift ${(summary.wf_agg_lift ?? 0) >= 0 ? '+' : ''}${formatPct(summary.wf_agg_lift)})`,
        },
        {
          label: 'Top-Bottom Q-Spread',
          value: formatPct(summary.quantile_spread, 3),
          sub: `Cum Spread: ${formatPct(summary.cumulative_q_spread, 1)} · MCC ${formatNum(summary.wf_agg_mcc, 3)}`,
        },
        {
          label: 'Model Top-N vs Vol Control',
          value: `${formatNum(summary.model_topn_sharpe, 2)} vs ${formatNum(summary.vol_topn_sharpe, 2)}`,
          sub: `Shuffled: ${formatNum(summary.shuffled_signal_sharpe, 2)} · Rand-Vol: ${formatNum(summary.rand_vol_topk_sharpe, 2)}`,
          badge: (summary.model_topn_sharpe ?? 0) > (summary.vol_topn_sharpe ?? 0) + 0.15 ? 'REAL SKILL' : 'CONTROL',
          badgeType: (summary.model_topn_sharpe ?? 0) > (summary.vol_topn_sharpe ?? 0) + 0.15 ? 'sentiment' : 'no-sentiment',
        },
        {
          label: 'Fake-Alpha Vol Bias & HV-IC',
          value: formatNum(summary.pred_vol_rank_corr, 3),
          sub: `Corr(Pred,Vol) · High-Vol Pool IC: ${formatNum(summary.high_vol_pool_ic, 4)}`,
        },
        {
          label: 'Timing Alpha & Robustness',
          value: `${(summary.timing_alpha_pct ?? 0) >= 0 ? '+' : ''}${formatNum(summary.timing_alpha_pct, 1)}%`,
          sub: `Ex-Top3 Sharpe: ${formatNum(summary.sharpe_ex_top3_stocks, 2)} · Ex-Best5d: ${formatNum(summary.sharpe_ex_best5_days, 2)}`,
        },
        {
          label: 'Leakage & Pre-Deploy Sanity',
          value: summary.lookahead_passed === 1 ? 'SEALED ✔' : 'CHECK ⚠',
          sub: `Overfit: ${summary.synthetic_overfit_passed === 1 ? 'OK' : 'FAIL'} · SentVar: ${summary.sentiment_var_passed === 1 ? 'OK' : 'FAIL'} · p=${formatNum(summary.signal_pvalue, 4)}`,
          badge: `${formatInt(summary.evaluated_days || 0)}d · ${summary.n_assets || config?.n_assets || '?'} eq`,
        },
      ];

  return (
    <div className="metrics-grid">
      {cards.map((c, i) => (
        <MetricCard key={i} {...c} />
      ))}
    </div>
  );
}
