import React from 'react';
import { renderHook, act } from '@testing-library/react';
import { describe, test, expect, beforeEach, vi } from 'vitest';

import useStatusSocket from '../useStatusSocket.js';
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

describe('useStatusSocket', () => {
  test('connects to the device websocket and starts connecting', () => {
    const { result } = renderHook(() => useStatusSocket());
    const ws = MockWebSocket.instances[0];
    expect(ws).toBeDefined();
    expect(ws.url).toBe('ws://' + window.location.host + '/ws');
    expect(result.current.connection).toBe('connecting');
    expect(result.current.stale).toBe(false);
    expect(result.current.msgAgeMs).toBeNull();
    expect(result.current.telemetryState).toBeNull();
    expect(result.current.gnssState).toBeNull();
  });

  test('open + status message yields a healthy, up-to-date socket', () => {
    const { result } = renderHook(() => useStatusSocket());
    const ws = MockWebSocket.instances[0];
    act(() => {
      ws.onopen();
      ws.onmessage({ data: JSON.stringify(fixture) });
    });
    expect(result.current.connection).toBe('connected');
    expect(result.current.stale).toBe(false);
    expect(result.current.msgAgeMs).not.toBeNull();
    expect(result.current.telemetryState).toBe(fixture.telemetry);
    expect(result.current.gnssState).toBe(fixture.gnss);
    expect(result.current.txState).toEqual(fixture.tx);
  });

  test('exposes the transmit rates only once the payload carries them', () => {
    const { result } = renderHook(() => useStatusSocket());
    const ws = MockWebSocket.instances[0];
    expect(result.current.txState).toBeNull();
    act(() => {
      ws.onopen();
      // Older firmware: status message without the tx diagnostics.
      ws.onmessage({ data: JSON.stringify({ type: 'status', telemetry: true, gnss: false }) });
    });
    expect(result.current.txState).toBeNull();
    act(() => {
      ws.onmessage({ data: JSON.stringify(fixture) });
    });
    expect(result.current.txState).toEqual(fixture.tx);
  });

  test('ignores junk and non-status messages, then still updates', () => {
    const { result } = renderHook(() => useStatusSocket());
    const ws = MockWebSocket.instances[0];
    act(() => {
      ws.onmessage({ data: 'not json' });
    });
    act(() => {
      ws.onmessage({ data: JSON.stringify({ type: 'other', telemetry: true, gnss: true }) });
    });
    expect(result.current.telemetryState).toBeNull();
    expect(result.current.gnssState).toBeNull();

    act(() => {
      ws.onmessage({ data: JSON.stringify(fixture) });
    });
    expect(result.current.telemetryState).toBe(fixture.telemetry);
    expect(result.current.gnssState).toBe(fixture.gnss);
  });

  test('flags an open but silent socket as stale', () => {
    vi.useFakeTimers();
    try {
      const { result } = renderHook(() => useStatusSocket());
      const ws = MockWebSocket.instances[0];
      act(() => {
        ws.onopen();
        ws.onmessage({ data: JSON.stringify(fixture) });
      });
      expect(result.current.stale).toBe(false);

      // The firmware pushes once per second; STALE_AFTER_MS of silence from
      // an open socket must flip stale.
      act(() => {
        vi.advanceTimersByTime(6000);
      });
      expect(result.current.stale).toBe(true);
      expect(Math.floor(result.current.msgAgeMs / 1000)).toBeGreaterThanOrEqual(5);
    } finally {
      vi.useRealTimers();
    }
  });

  test('a freshly opened socket is stale until its first message', () => {
    const { result } = renderHook(() => useStatusSocket());
    const ws = MockWebSocket.instances[0];
    act(() => {
      ws.onopen();
    });
    // Open but no status message yet: that IS no-data — the box must not
    // show green first and flip later. It clears with the first push.
    expect(result.current.connection).toBe('connected');
    expect(result.current.stale).toBe(true);
    expect(result.current.msgAgeMs).toBeNull();

    act(() => {
      ws.onmessage({ data: JSON.stringify(fixture) });
    });
    expect(result.current.stale).toBe(false);
    expect(result.current.msgAgeMs).not.toBeNull();
    expect(result.current.msgAgeMs).toBeGreaterThanOrEqual(0);
  });

  test('shows closed after the socket closes, with the age since', () => {
    vi.useFakeTimers();
    try {
      const { result } = renderHook(() => useStatusSocket());
      const ws = MockWebSocket.instances[0];
      act(() => {
        ws.onopen();
      });
      // While connected there is no disconnection age.
      expect(result.current.closedAgeMs).toBeNull();

      act(() => {
        ws.onclose();
      });
      expect(result.current.connection).toBe('closed');
      expect(result.current.closedAgeMs).not.toBeNull();
      expect(result.current.closedAgeMs).toBeGreaterThanOrEqual(0);

      act(() => {
        vi.advanceTimersByTime(5000);
      });
      expect(result.current.connection).toBe('closed');
      expect(Math.floor(result.current.closedAgeMs / 1000)).toBeGreaterThanOrEqual(5);
    } finally {
      vi.useRealTimers();
    }
  });

  test('shows closed after an error too', () => {
    const { result } = renderHook(() => useStatusSocket());
    const ws = MockWebSocket.instances[0];
    expect(typeof ws.onerror).toBe('function');
    expect(() => act(() => ws.onerror(new Error('boom')))).not.toThrow();
    expect(result.current.connection).toBe('closed');
    expect(result.current.closedAgeMs).not.toBeNull();
  });

  test('closes the websocket on unmount', () => {
    const { unmount } = renderHook(() => useStatusSocket());
    const ws = MockWebSocket.instances[0];
    expect(ws.closed).toBe(false);
    act(() => {
      unmount();
    });
    expect(ws.closed).toBe(true);
  });
});
