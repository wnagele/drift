import React from 'react';
import { DashboardOutlined, EnvironmentOutlined } from '@ant-design/icons';
import ConnectionBox from './ConnectionBox';
import StatusBox from './StatusBox';

// The device-health flags from the latest status message. Each is a rolling
// 3 s window in the firmware (status_process): true means at least one
// qualifying message arrived in the last window.
//
//   telemetry — a decoded MAVLink HEARTBEAT, i.e. the flight-controller link
//   gnss      — a GLOBAL_POSITION_INT accepted *while* the fix was 3D, so it
//               reports position updates and fix quality together; a false
//               reading does not distinguish "no fix" from "no position
//               messages", which is one reason these boxes carry no detail
//               line: any wording for the negative case would have to pick
//               one of the two and would be wrong half the time.
//
// So the colour and the name are the whole message. The connection box next
// to them does carry a detail line, because an age ("updated 3s ago") is
// information the colour cannot express.
//
// `icon` is only shown on a collapsed sider, where there is no room for the
// name: a gauge for instrument data off the flight controller, a map pin for
// a position fix.
const FLAGS = [
  { key: 'telemetry', label: 'Telemetry', icon: <DashboardOutlined /> },
  { key: 'gnss', label: 'GNSS', icon: <EnvironmentOutlined /> },
];

// null/undefined is "no status message yet" — deliberately not red, which
// would claim a fault the device has not reported.
const flagState = (value) => {
  switch (value) {
    case true:
      return 'ok';
    case false:
      return 'down';
    default:
      return 'degraded';
  }
};

// The sidebar health block: link health plus what the device reports over it,
// in one always-visible column. It lives in the sider rather than in a view
// on purpose — these are the answers you want while editing config or
// reading the broadcast values, not a place you navigate to. Collapses to a
// stack of tooltipped dots when the sider folds on a narrow screen.
const SidebarHealth = ({ status, collapsed = false }) => {
  const values = { telemetry: status.telemetryState, gnss: status.gnssState };

  return (
    <div className={collapsed ? 'status-boxes status-boxes-collapsed' : 'status-boxes'}>
      <ConnectionBox
        collapsed={collapsed}
        connection={status.connection}
        stale={status.stale}
        msgAgeMs={status.msgAgeMs}
        closedAgeMs={status.closedAgeMs}
      />
      {FLAGS.map((flag) => (
        <StatusBox
          key={flag.key}
          collapsed={collapsed}
          icon={flag.icon}
          state={flagState(values[flag.key])}
          label={flag.label}
        />
      ))}
    </div>
  );
};

export default SidebarHealth;
