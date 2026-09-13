import React from 'react';
import { render } from '@testing-library/react';
import { describe, test, expect } from 'vitest';

import Status from '../Status.js';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';

function fixture_read(rel) {
  const paths = [join(process.cwd(), '../test/fixtures', rel), join(process.cwd(), 'test/fixtures', rel)];
  for (const path of paths) {
    try {
      return JSON.parse(readFileSync(path, 'utf8'));
    } catch {
      // try next candidate
    }
  }
  throw new Error(`fixture not found: ${rel}`);
}

const fixture = fixture_read('api/status.json');
const broadcast = fixture_read('api/broadcast.json');

// The view renders two tables; the assertions below are scoped to one of
// them, because a dash in the transmit rates and a dash in the broadcast
// content mean different things.
function cellTexts(container, index = 0) {
  const table = container.querySelectorAll('.ant-table')[index];
  return Array.from(table.querySelectorAll('.ant-table-cell'))
    .map((cell) => cell.textContent);
}

// Value of one broadcast row, looked up by its label. The table has no
// header, so the two columns are label and value.
function broadcastRow(container, label) {
  const table = container.querySelectorAll('.ant-table')[1];
  for (const row of table.querySelectorAll('.ant-table-row')) {
    const cells = row.querySelectorAll('.ant-table-cell');
    if (cells[0].textContent === label)
      return cells[1].textContent;
  }
  return null;
}

function broadcastLabels(container) {
  const table = container.querySelectorAll('.ant-table')[1];
  return Array.from(table.querySelectorAll('.ant-table-row'))
    .map((row) => row.querySelectorAll('.ant-table-cell')[0].textContent);
}

describe('Status', () => {
  test('renders every transport with dashes before the first status message', () => {
    const { container } = render(<Status txState={null} broadcastState={null} />);
    expect(container.querySelector('.ant-table')).not.toBeNull();
    expect(container.textContent).toContain('Transmit rates');
    for (const label of ['Bluetooth 4 legacy', 'Bluetooth 5 Long Range', 'Wi-Fi Beacon', 'Wi-Fi NAN']) {
      expect(container.textContent).toContain(label);
    }
    // Unknown values dash out instead of showing a misleading zero.
    const texts = cellTexts(container);
    expect(texts.filter((t) => t === '—')).toHaveLength(8);
  });

  test('renders per-transport transmit rates from the status message', () => {
    const { container } = render(<Status txState={fixture.tx} broadcastState={null} />);
    expect(container.querySelector('.ant-table')).not.toBeNull();
    for (const label of ['Bluetooth 4 legacy', 'Bluetooth 5 Long Range', 'Wi-Fi Beacon', 'Wi-Fi NAN']) {
      expect(container.textContent).toContain(label);
    }
    // Fixture values render verbatim: the bench steady state (all ten BT4
    // slots, N=3 packs, five IE refreshes, two NAN action frames).
    const texts = cellTexts(container);
    expect(texts).toContain('10');
    expect(texts).toContain('15');
    expect(texts).toContain('6');
  });

  test('dashes out transports the payload does not carry', () => {
    const { container } = render(<Status txState={{ bt4: { frames: 1, messages: 1 } }} broadcastState={null} />);
    expect(container.textContent).toContain('Bluetooth 4 legacy');
    const texts = cellTexts(container);
    expect(texts).toContain('1');
    expect(texts.filter((t) => t === '—')).toHaveLength(6);
  });

  test('carries no health indicators — those live in the sidebar', () => {
    // Device and link health moved to SidebarHealth so they stay visible
    // from every tab; this view is what the device puts on air.
    const { container } = render(<Status txState={fixture.tx} broadcastState={broadcast} />);
    expect(container.querySelector('.status-box')).toBeNull();
    for (const table of [0, 1]) {
      const texts = cellTexts(container, table);
      for (const label of ['Telemetry', 'GNSS', 'Connected'])
        expect(texts).not.toContain(label);
    }
  });

  describe('broadcast content', () => {
    test('lists nothing before the first broadcast message', () => {
      // Older firmware without the payload looks the same as "not yet": the
      // dash has no answer, and an empty list plus "no data yet" says so.
      const { container } = render(<Status txState={fixture.tx} broadcastState={null} />);
      expect(container.textContent).toContain('Broadcast content');
      expect(container.textContent).toContain('no data yet');
      expect(broadcastLabels(container)).toHaveLength(0);
    });

    test('renders the shared fixture payload with its units', () => {
      const { container } = render(<Status txState={fixture.tx} broadcastState={broadcast} />);
      expect(broadcastRow(container, 'Flight status')).toBe('Airborne');
      expect(broadcastRow(container, 'Aircraft type')).toBe('None');
      expect(broadcastRow(container, 'Course over ground')).toBe('90°');
      expect(broadcastRow(container, 'Ground speed')).toBe('3.5 m/s');
      expect(broadcastRow(container, 'Vertical speed')).toBe('2.5 m/s');
    });

    test('reads a coordinate triple and the accuracies as one row each', () => {
      // Three rows for a position, and three more saying "Unknown", is the
      // firmware's data model rather than anything an operator reads one
      // line at a time. The parts are named inside the value, which is what
      // keeps them legible when one of them has no value.
      const { container } = render(<Status txState={null} broadcastState={broadcast} />);
      expect(broadcastRow(container, 'Location'))
        .toBe('Latitude 47.3566123°, Longitude 8.5321456°, Altitude 500.5 m');
      expect(broadcastRow(container, 'Accuracy'))
        .toBe('Horizontal Unknown, Vertical Unknown, Speed Unknown');
      expect(broadcastRow(container, 'Operator location'))
        .toBe('Latitude 47.3566°, Longitude 8.5321°, Altitude 500 m (Type: Take-off)');

      // The altitudes carry no datum in their label: F3411 declares the
      // field as WGS-84 ellipsoidal height and the flight controller
      // supplies height above the geoid, so anything more specific than
      // "Altitude" would be a claim the payload cannot support.
      const labels = broadcastLabels(container);
      for (const gone of ['Latitude', 'Longitude', 'Altitude', 'Altitude (MSL)',
                          'Horizontal accuracy', 'Take-off latitude'])
        expect(labels).not.toContain(gone);
    });

    test('shows a type with its value, not on a row of its own', () => {
      // A type without its subject is not something the device broadcasts,
      // and the operator does not need a second row to read one word. That
      // holds for the two enums that qualify a whole row as much as for the
      // ID types - the height's reference point and the kind of operator
      // location - so neither is in a label.
      const { container } = render(<Status txState={null} broadcastState={broadcast} />);
      expect(broadcastRow(container, 'Aircraft ID'))
        .toBe(broadcast.uas_id + ' (Type: Serial Number)');
      expect(broadcastRow(container, 'Operator ID'))
        .toBe(broadcast.operator_id + ' (Type: Operator ID)');
      expect(broadcastRow(container, 'Height')).toBe('30.5 m (Type: Above Take-off)');
      const labels = broadcastLabels(container);
      for (const gone of ['ID type', 'Description type', 'Height reference',
                          'Height above take-off', 'Operator location type'])
        expect(labels).not.toContain(gone);
    });

    test('reads the type strings out of the payload, holding no vocabulary', () => {
      // A device reporting a live operator position or a height above
      // ground relabels nothing and needs no dash change; no firmware
      // produces either today, so this is the only place it is exercised.
      const live = { ...broadcast, operator_location_type: 'Live GNSS',
                     height_type: 'Above Ground' };
      const { container } = render(<Status txState={null} broadcastState={live} />);
      expect(broadcastRow(container, 'Height')).toBe('30.5 m (Type: Above Ground)');
      expect(broadcastRow(container, 'Operator location'))
        .toBe('Latitude 47.3566°, Longitude 8.5321°, Altitude 500 m (Type: Live GNSS)');
    });

    test('omits what is not being broadcast, rather than naming it', () => {
      // The fixture configures no Self-ID, so that message is not on air -
      // and the table is a list of what goes out, so it is simply absent.
      // The cost of that, deliberately accepted: an identity DRIFT silently
      // dropped (an over-length serial) shows up as a missing row, not as a
      // statement.
      const { container } = render(<Status txState={fixture.tx} broadcastState={broadcast} />);
      const labels = broadcastLabels(container);
      expect(labels).not.toContain('Description');
      expect(labels).toEqual(['Aircraft ID', 'Aircraft type', 'Flight status',
                              'Location', 'Height', 'Course over ground',
                              'Ground speed', 'Vertical speed', 'Accuracy',
                              'Operator location', 'Operator ID']);
      expect(cellTexts(container, 1)).not.toContain('not broadcast');
    });

    test('an explicit null reads "unknown"', () => {
      // The fresh-device payload: location broadcast from power-on, every
      // measurement carrying ODID's "no value". A receiver renders these as
      // blanks with nothing to say why; this is where they become legible.
      // The four unconfigured messages contribute no rows at all, which is
      // how a user sees that their identity config never took effect.
      const fresh = {
        type: 'broadcast',
        flight_status: 'Undeclared',
        latitude: null,
        longitude: null,
        altitude_geo: null,
        height: null,
        height_type: 'Above Take-off',
        direction: null,
        speed_horizontal: null,
        speed_vertical: null,
        horiz_accuracy: 'Unknown',
        vert_accuracy: 'Unknown',
        speed_accuracy: 'Unknown',
      };
      const { container } = render(<Status txState={null} broadcastState={fresh} />);
      expect(broadcastRow(container, 'Location'))
        .toBe('Latitude unknown, Longitude unknown, Altitude unknown');
      expect(broadcastRow(container, 'Course over ground')).toBe('unknown');
      expect(broadcastRow(container, 'Height')).toBe('unknown (Type: Above Take-off)');
      // An accuracy of "Unknown" is a value the device does transmit, so it
      // stays the payload's own word.
      expect(broadcastRow(container, 'Accuracy'))
        .toBe('Horizontal Unknown, Vertical Unknown, Speed Unknown');
      const labels = broadcastLabels(container);
      for (const gone of ['Aircraft ID', 'Aircraft type', 'Description',
                          'Operator ID', 'Operator location'])
        expect(labels).not.toContain(gone);
    });

    test('reports the age of the last broadcast message', () => {
      // From the WebSocket receive time, which is why the payload carries no
      // timestamp of its own.
      const { container } = render(
        <Status txState={null} broadcastState={broadcast} broadcastAgeMs={2400} />);
      expect(container.textContent).toContain('updated 2s ago');
    });
  });
});
