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

// Statistics view: the per-transport transmit rates from the latest status
// message, riding the same /ws connection the Status view uses (owned by
// useStatusSocket in App, so it stays connected across tab switches).
const Statistics = ({ txState }) => {
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

export default Statistics;
