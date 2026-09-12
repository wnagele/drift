import React from 'react';
import { Table, Typography } from 'antd';

// Transmit-rate rows, one per transport in broadcast-schedule order. The
// values come straight from the status message's tx object (txcount.cpp:
// Remote-ID-carrying frames the schedule handed to the radio seams over the
// last second, and the ODID messages they carried); a missing transport or
// field — before the first status message, or from older firmware without
// the tx diagnostics — renders as a dash rather than a misleading zero.
const TRANSPORTS = [
  { key: 'bt4', label: 'Bluetooth 4 legacy' },
  { key: 'bt5', label: 'Bluetooth 5 Long Range' },
  { key: 'wifi_beacon', label: 'Wi-Fi Beacon' },
  { key: 'wifi_nan', label: 'Wi-Fi NAN' },
];

const TX_COLUMNS = [
  { title: 'Transport', dataIndex: 'transport' },
  { title: 'Frames/s', dataIndex: 'frames' },
  { title: 'Messages/s', dataIndex: 'messages' },
];

// Status view: what the device is putting on air. Sectioned, because this is
// where the broadcast values land next to the transmit rates. Device and link
// health are deliberately not here — they live in the sidebar so they stay
// visible from every tab (SidebarHealth), which is also why this view carries
// no /ws plumbing of its own: App owns the one socket and passes the data in.
const Status = ({ txState }) => {
  const txRows = TRANSPORTS.map(({ key, label }) => ({
    key,
    transport: label,
    frames: txState?.[key]?.frames ?? '—',
    messages: txState?.[key]?.messages ?? '—',
  }));

  return (
    <>
      <Typography.Title level={5}>Transmit rates</Typography.Title>
      <Table size="small" pagination={false} columns={TX_COLUMNS} dataSource={txRows} />
    </>
  );
};

export default Status;
