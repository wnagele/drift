import React from 'react';
import { ApiOutlined } from '@ant-design/icons';
import StatusBox from './StatusBox';

// Connection indicator in the left navigation — the single home for /ws
// connection health. The box color and state word flip between green
// (Connected), orange (Connecting / No data) and red (Disconnected), with a
// detail line for the most recent update age (or how long ago the
// connection was lost). Connection only: whether the dash can hear the
// device at all, which is a different question from what the device is
// reporting once heard (the telemetry/GNSS flags beside it).
//
// The collapsed-sider icon is a plug rather than a Wi-Fi glyph on purpose:
// this box measures the /ws socket, and a Wi-Fi symbol would invite reading
// it as radio signal strength, which it is not.
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

  return (
    <StatusBox
      collapsed={collapsed}
      icon={<ApiOutlined />}
      state={state()}
      label={title()}
      detail={detail()}
    />
  );
};

export default ConnectionBox;
