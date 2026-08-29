import React from 'react';

// Connection indicator in the left navigation — the single home for /ws
// connection health. The box color and state word flip between green
// (Connected), orange (Connecting / No data) and red (Disconnected), with a
// detail line for the most recent update age (or how long ago the
// connection was lost). Connection only; telemetry/GNSS live in the Status
// view. Collapses to a colored dot with a native tooltip when the sider
// folds.
const ConnectionBox = ({ connection, stale, msgAgeMs, closedAgeMs, collapsed = false }) => {
  const state = () => {
    if (connection === 'closed')
      return 'down';
    if (connection !== 'connected' || stale)
      return 'degraded';
    return 'ok';
  };

  const title = () => {
    switch (connection) {
      case 'connecting':
        return 'Connecting…';
      case 'closed':
        return 'Disconnected';
      default:
        return stale ? 'No data' : 'Connected';
    }
  };

  // Detail line: the age of the last update while connected, or how long
  // ago the connection was lost. Ages can never be negative — clamped both
  // in the hook and here, so a stale `now` tick can never render "-1s ago".
  const secs = (ms) => Math.max(0, Math.floor(ms / 1000));

  const detail = () => {
    if (connection === 'closed')
      return closedAgeMs == null
        ? null
        : 'since ' + secs(closedAgeMs) + 's ago';
    if (connection !== 'connected')
      return null;
    if (stale)
      return msgAgeMs === null
        ? 'no message yet'
        : 'last update ' + secs(msgAgeMs) + 's ago';
    return 'updated ' + secs(msgAgeMs) + 's ago';
  };

  const s = state();
  const t = title();
  const d = detail();
  const tooltip = d ? t + ' · ' + d : t;

  if (collapsed)
    return <div className={'status-box-collapsed status-dot-' + s} title={tooltip} />;

  return (
    <div className={'status-box status-box-' + s}>
      <div className="status-box-line status-connection">
        <span className={'status-dot status-dot-' + s} />
        {t}
      </div>
      {d && <div className="status-box-detail">{d}</div>}
    </div>
  );
};

export default ConnectionBox;
