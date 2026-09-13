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

// The broadcast inspector rows: one per thing the device is transmitting.
// The payload is flat and so is this - which ODID message carries a field is
// the firmware's business, not the operator's, so nothing here names the
// messages.
//
// `unit` is a verbatim suffix, so it carries its own spacing. `typeKey` names
// the enum that qualifies the row, rendered after the value rather than on a
// row of its own ("Aircraft ID: ... (Type: Serial Number)"): a type without
// its subject is not a thing the device broadcasts.
//
// `parts` is one row built from several payload fields, each named inside the
// value. The three it is used for are read as a unit anyway - a coordinate
// triple, and the accuracy trio, which would otherwise be three rows saying
// "Unknown" - and naming the parts inside the value is what keeps them
// legible when one of them is broadcast as "no value".
//
// The altitudes are deliberately not labelled with a datum. ASTM F3411
// declares both as WGS-84 ellipsoidal height while the flight controller
// supplies height above the geoid (~47 m apart in central Europe), so the
// honest label until that is fixed at the source is the bare word.
const BROADCAST_FIELDS = [
  { key: 'uas_id', label: 'Aircraft ID', typeKey: 'uas_id_type' },
  { key: 'ua_type', label: 'Aircraft type' },
  { key: 'flight_status', label: 'Flight status' },
  { label: 'Location', parts: [
    { key: 'latitude', name: 'Latitude', unit: '°' },
    { key: 'longitude', name: 'Longitude', unit: '°' },
    { key: 'altitude_geo', name: 'Altitude', unit: ' m' },
  ] },
  { key: 'height', label: 'Height', unit: ' m', typeKey: 'height_type' },
  { key: 'direction', label: 'Course over ground', unit: '°' },
  { key: 'speed_horizontal', label: 'Ground speed', unit: ' m/s' },
  { key: 'speed_vertical', label: 'Vertical speed', unit: ' m/s' },
  { label: 'Accuracy', parts: [
    { key: 'horiz_accuracy', name: 'Horizontal' },
    { key: 'vert_accuracy', name: 'Vertical' },
    { key: 'speed_accuracy', name: 'Speed' },
  ] },
  { key: 'desc', label: 'Description', typeKey: 'desc_type' },
  { label: 'Operator location', typeKey: 'operator_location_type', parts: [
    { key: 'operator_latitude', name: 'Latitude', unit: '°' },
    { key: 'operator_longitude', name: 'Longitude', unit: '°' },
    { key: 'operator_altitude_geo', name: 'Altitude', unit: ' m' },
  ] },
  { key: 'operator_id', label: 'Operator ID', typeKey: 'operator_id_type' },
];

// Two columns, no header: a label beside its value needs no column names.
const BROADCAST_COLUMNS = [
  { dataIndex: 'field' },
  { dataIndex: 'value' },
];

// A field the payload omits is one the device is not broadcasting, and the
// table simply does not list it - the table is what goes out. An explicit
// null is different and does get a row: the field *is* broadcast, carrying
// ODID's in-band "no value", which a receiver renders as a blank with
// nothing to say why.
const keys = (field) => (field.parts ? field.parts.map((p) => p.key) : [field.key]);

const shown = (state, { key, unit }) => (state[key] === null
  ? 'unknown'
  : (unit ? state[key] + unit : String(state[key])));

const value = (state, field) => {
  const body = field.parts
    ? field.parts.filter((part) => part.key in state)
        .map((part) => part.name + ' ' + shown(state, part)).join(', ')
    : shown(state, field);
  const type = field.typeKey ? state[field.typeKey] : undefined;
  return type ? body + ' (Type: ' + type + ')' : body;
};

// Status view: what the device is putting on air. Sectioned, because this is
// where the broadcast values land next to the transmit rates. Device and link
// health are deliberately not here — they live in the sidebar so they stay
// visible from every tab (SidebarHealth), which is also why this view carries
// no /ws plumbing of its own: App owns the one socket and passes the data in.
const Status = ({ txState, broadcastState, broadcastAgeMs }) => {
  const txRows = TRANSPORTS.map(({ key, label }) => ({
    key,
    transport: label,
    frames: txState?.[key]?.frames ?? '—',
    messages: txState?.[key]?.messages ?? '—',
  }));

  const broadcast = broadcastState ?? {};
  const broadcastRows = BROADCAST_FIELDS
    .filter((field) => keys(field).some((key) => key in broadcast))
    .map((field) => ({
      key: field.label,
      field: field.label,
      value: value(broadcast, field),
    }));


  // Freshness from the websocket receive time, which is why the payload
  // carries no timestamp of its own.
  const age = broadcastAgeMs == null
    ? 'no data yet'
    : 'updated ' + Math.max(0, Math.floor(broadcastAgeMs / 1000)) + 's ago';

  return (
    <>
      <Typography.Title level={5}>Transmit rates</Typography.Title>
      <Table size="small" pagination={false} columns={TX_COLUMNS} dataSource={txRows} />
      <Typography.Title level={5}>Broadcast content</Typography.Title>
      <Typography.Paragraph type="secondary">{age}</Typography.Paragraph>
      <Table size="small" pagination={false} showHeader={false}
             columns={BROADCAST_COLUMNS} dataSource={broadcastRows} />
    </>
  );
};

export default Status;
