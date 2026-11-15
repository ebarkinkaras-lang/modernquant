import React from 'react';

export default function Header({ experimentCount }) {
  return (
    <header className="header">
      <div className="header-brand">
        <div>
          <h1>ModernQuantSuite C++20 · Quantitative Research Terminal</h1>
          <span className="subtitle">
            Cross-Sectional Portfolio Engine · Sticky Sentiment Memory · Fake-Alpha &amp; Leakage Diagnostics
          </span>
        </div>
      </div>
      <div className="header-badge">
        <span className="dot" />
        {experimentCount} Reports Loaded
      </div>
    </header>
  );
}
