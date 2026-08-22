import React from 'react';
import { render, act } from '@testing-library/react';
import { describe, test, expect, beforeEach } from 'vitest';

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

// Records every constructed WebSocket so tests can drive messages into it.
class MockWebSocket {
  static instances = [];
  constructor(url) {
    this.url = url;
    this.closed = false;
    MockWebSocket.instances.push(this);
  }
  close() {
    this.closed = true;
  }
}

beforeEach(() => {
  MockWebSocket.instances = [];
  global.WebSocket = MockWebSocket;
});

function rows(container) {
  const rows = container.querySelectorAll('.ant-row');
  expect(rows).toHaveLength(2);
  return rows;
}

describe('Status', () => {
  test('connects to the device websocket and renders unknown states', () => {
    let container;
    act(() => {
      ({ container } = render(<Status />));
    });
    const ws = MockWebSocket.instances[0];
    expect(ws).toBeDefined();
    expect(ws.url).toBe('ws://' + window.location.host + '/ws');

    const [telemetryRow, gnssRow] = rows(container);
    expect(telemetryRow.textContent).toContain('Telemetry');
    expect(gnssRow.textContent).toContain('GNSS');
    expect(container.querySelectorAll('.anticon-question-circle')).toHaveLength(2);
    expect(container.querySelectorAll('.anticon-check-circle')).toHaveLength(0);
    expect(container.querySelectorAll('.anticon-close-circle')).toHaveLength(0);
  });

  test('maps each flag to its own row (telemetry ok, gnss failed)', () => {
    let container;
    act(() => {
      ({ container } = render(<Status />));
    });
    const ws = MockWebSocket.instances[0];
    act(() => {
      ws.onmessage({ data: JSON.stringify(fixture) });
    });
    const [telemetryRow, gnssRow] = rows(container);
    expect(telemetryRow.querySelector('.anticon-check-circle')).not.toBeNull();
    expect(telemetryRow.querySelector('.anticon-close-circle')).toBeNull();
    expect(gnssRow.querySelector('.anticon-close-circle')).not.toBeNull();
    expect(gnssRow.querySelector('.anticon-check-circle')).toBeNull();
  });

  test('ignores junk and non-status messages, then still updates', () => {
    let container;
    act(() => {
      ({ container } = render(<Status />));
    });
    const ws = MockWebSocket.instances[0];
    act(() => {
      ws.onmessage({ data: 'not json' });
    });
    act(() => {
      ws.onmessage({ data: JSON.stringify({ type: 'other', telemetry: true, gnss: true }) });
    });
    expect(container.querySelectorAll('.anticon-question-circle')).toHaveLength(2);

    act(() => {
      ws.onmessage({ data: JSON.stringify(fixture) });
    });
    const [telemetryRow, gnssRow] = rows(container);
    expect(telemetryRow.querySelector('.anticon-check-circle')).not.toBeNull();
    expect(gnssRow.querySelector('.anticon-close-circle')).not.toBeNull();
  });

  test('wires an error handler', () => {
    let container;
    act(() => {
      ({ container } = render(<Status />));
    });
    const ws = MockWebSocket.instances[0];
    expect(typeof ws.onerror).toBe('function');
    expect(() => act(() => ws.onerror(new Error('boom')))).not.toThrow();
  });

  test('closes the websocket on unmount', () => {
    let unmount;
    act(() => {
      ({ unmount } = render(<Status />));
    });
    const ws = MockWebSocket.instances[0];
    expect(ws.closed).toBe(false);
    act(() => {
      unmount();
    });
    expect(ws.closed).toBe(true);
  });
});
