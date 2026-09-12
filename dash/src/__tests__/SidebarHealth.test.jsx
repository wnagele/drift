import React from 'react';
import { render } from '@testing-library/react';
import { describe, test, expect } from 'vitest';

import SidebarHealth from '../SidebarHealth.js';
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

// A healthy socket, so the connection box never colours the flags' own
// assertions by accident.
const connected = {
  connection: 'connected',
  stale: false,
  msgAgeMs: 0,
  closedAgeMs: null,
};

const health = (overrides) => ({ ...connected, ...overrides });

function boxes(container) {
  return [...container.querySelectorAll('.status-box')];
}

function box(container, label) {
  const found = boxes(container).find(
    (b) => b.querySelector('.status-box-label').textContent === label
  );
  expect(found, 'no box labelled ' + label).not.toBeUndefined();
  return found;
}

describe('SidebarHealth', () => {
  test('renders link health and both device flags as sider boxes', () => {
    const { container } = render(
      <SidebarHealth status={health({ telemetryState: true, gnssState: true })} />
    );
    expect(boxes(container)).toHaveLength(3);
    for (const label of ['Connected', 'Telemetry', 'GNSS'])
      expect(box(container, label)).not.toBeUndefined();
  });

  test('renders unknown flag states before the first status message', () => {
    // Not red: the device has not reported a fault, we simply have no
    // reading yet.
    const { container } = render(
      <SidebarHealth status={health({ telemetryState: null, gnssState: null })} />
    );
    for (const label of ['Telemetry', 'GNSS'])
      expect(box(container, label).className).toContain('status-box-degraded');
  });

  test('the flags carry no detail line — the colour and name say it all', () => {
    // Only the connection box has something a colour cannot express (an
    // age). A flag's negative case cannot be worded honestly anyway: gnss
    // false conflates "no fix" with "no position messages".
    const { container } = render(
      <SidebarHealth status={health({ telemetryState: true, gnssState: false })} />
    );
    for (const label of ['Telemetry', 'GNSS']) {
      const b = box(container, label);
      expect(b.querySelector('.status-box-detail')).toBeNull();
      expect(b.textContent).toBe(label);
    }
    expect(box(container, 'Connected').querySelector('.status-box-detail')).not.toBeNull();
  });

  test('maps each flag to its own box (telemetry ok, gnss failed)', () => {
    // The shared fixture is asymmetric on purpose, so a telemetry/gnss swap
    // cannot cancel out.
    expect(fixture.telemetry).toBe(true);
    expect(fixture.gnss).toBe(false);

    const { container } = render(
      <SidebarHealth
        status={health({ telemetryState: fixture.telemetry, gnssState: fixture.gnss })}
      />
    );

    const telemetry = box(container, 'Telemetry');
    expect(telemetry.className).toContain('status-box-ok');
    expect(telemetry.querySelector('.status-dot-ok')).not.toBeNull();

    const gnss = box(container, 'GNSS');
    expect(gnss.className).toContain('status-box-down');
    expect(gnss.querySelector('.status-dot-down')).not.toBeNull();

    // A failed flag is the device's report, not the link's — the connection
    // box must stay green.
    expect(box(container, 'Connected').className).toContain('status-box-ok');
  });

  test('a lost connection does not turn the flags red', () => {
    // The flags keep the last value the device reported; inventing a fault
    // for them would hide the fact that the real problem is the link.
    const { container } = render(
      <SidebarHealth
        status={{
          connection: 'closed',
          stale: false,
          msgAgeMs: 1000,
          closedAgeMs: 2000,
          telemetryState: true,
          gnssState: true,
        }}
      />
    );
    expect(box(container, 'Disconnected').className).toContain('status-box-down');
    expect(box(container, 'Telemetry').className).toContain('status-box-ok');
    expect(box(container, 'GNSS').className).toContain('status-box-ok');
  });

  test('collapses to an icon plus a state dot per box', () => {
    const { container } = render(
      <SidebarHealth
        collapsed={true}
        status={health({ telemetryState: true, gnssState: false })}
      />
    );
    expect(container.querySelector('.status-boxes-collapsed')).not.toBeNull();
    expect(container.querySelectorAll('.status-box')).toHaveLength(0);

    const items = [...container.querySelectorAll('.status-box-collapsed')];
    expect(items).toHaveLength(3);
    // The flags tooltip to their bare name, since that is all they carry.
    expect(items.map((i) => i.title)).toEqual([
      'Connected · updated 0s ago',
      'Telemetry',
      'GNSS',
    ]);
    // Each keeps its own identity icon, so the collapsed block is readable
    // without hovering — three bare dots would not be.
    expect(items[0].querySelector('.anticon-api')).not.toBeNull();
    expect(items[1].querySelector('.anticon-dashboard')).not.toBeNull();
    expect(items[2].querySelector('.anticon-environment')).not.toBeNull();
    // …and the state still rides a dot beside the icon.
    expect(items[1].querySelector('.status-dot-ok')).not.toBeNull();
    expect(items[2].querySelector('.status-dot-down')).not.toBeNull();
  });
});
