// ReconnectingSocket owns the when-to-connect policy of the /ws link: the
// backoff schedule, the connect-attempt timeout, the visibility-triggered
// reconnect and the replaced-socket event guards. These tests pin that
// machinery directly, without the React hook around it.
import { describe, test, expect, beforeEach, afterEach, vi } from 'vitest';

import ReconnectingSocket, {
  RETRY_BASE_MS,
  CONNECT_TIMEOUT_MS,
} from '../reconnecting_socket.js';

// Records every constructed WebSocket so tests can drive it. readyState
// and the CONNECTING..CLOSED statics mirror the browser API the visibility
// handler branches on.
class MockWebSocket {
  static CONNECTING = 0;
  static OPEN = 1;
  static CLOSING = 2;
  static CLOSED = 3;
  static instances = [];
  constructor(url) {
    this.url = url;
    this.readyState = MockWebSocket.CONNECTING;
    this.closed = false;
    MockWebSocket.instances.push(this);
  }
  close() {
    this.closed = true;
    this.readyState = MockWebSocket.CLOSED;
  }
}

// Every created socket, so afterEach can close them: an unclosed socket
// keeps its document-level visibilitychange listener, and a later test's
// dispatch would fire the leaked listener too.
const live = [];

beforeEach(() => {
  MockWebSocket.instances = [];
  global.WebSocket = MockWebSocket;
});

afterEach(() => {
  for (const socket of live)
    socket.close();
  live.length = 0;
});

const newSocket = () => {
  const socket = new ReconnectingSocket('ws://' + window.location.host + '/ws');
  socket.onopen = vi.fn();
  socket.onclose = vi.fn();
  socket.onerror = vi.fn();
  socket.onmessage = vi.fn();
  live.push(socket);
  return socket;
};

const visible = () => {
  Object.defineProperty(document, 'visibilityState', {
    value: 'visible',
    configurable: true,
  });
};

describe('ReconnectingSocket', () => {
  test('connects immediately and forwards the native events', () => {
    const socket = newSocket();
    const ws = MockWebSocket.instances[0];
    expect(ws).toBeDefined();
    expect(ws.url).toBe('ws://' + window.location.host + '/ws');

    ws.onopen();
    ws.onmessage({ data: 'payload' });
    expect(socket.onopen).toHaveBeenCalledTimes(1);
    expect(socket.onmessage).toHaveBeenCalledWith({ data: 'payload' });
  });

  test('retries after a close, then resets once a socket opens', () => {
    vi.useFakeTimers();
    try {
      const socket = newSocket();
      MockWebSocket.instances[0].onopen();
      MockWebSocket.instances[0].onclose();
      expect(socket.onclose).toHaveBeenCalledTimes(1);

      vi.advanceTimersByTime(RETRY_BASE_MS);
      expect(MockWebSocket.instances.length).toBe(2);
      // No close spam while the new attempt is in flight.
      expect(socket.onclose).toHaveBeenCalledTimes(1);

      MockWebSocket.instances[1].onopen();
      MockWebSocket.instances[1].onclose();
      // A successful open reset the backoff, so the next retry waits the
      // base delay again, not a doubled one.
      vi.advanceTimersByTime(RETRY_BASE_MS - 1);
      expect(MockWebSocket.instances.length).toBe(2);
      vi.advanceTimersByTime(1);
      expect(MockWebSocket.instances.length).toBe(3);
    } finally {
      vi.useRealTimers();
    }
  });

  test('backs off exponentially while the link stays down', () => {
    vi.useFakeTimers();
    try {
      newSocket();
      // Every attempt fails without ever opening, so each close doubles
      // the next delay: 1 s, then 2 s.
      MockWebSocket.instances[0].onclose();
      vi.advanceTimersByTime(RETRY_BASE_MS);
      expect(MockWebSocket.instances.length).toBe(2);
      MockWebSocket.instances[1].onclose();
      vi.advanceTimersByTime(RETRY_BASE_MS * 2 - 1);
      expect(MockWebSocket.instances.length).toBe(2);
      vi.advanceTimersByTime(1);
      expect(MockWebSocket.instances.length).toBe(3);
    } finally {
      vi.useRealTimers();
    }
  });

  test('cuts short a connect attempt that never opens', () => {
    vi.useFakeTimers();
    try {
      newSocket();
      const ws = MockWebSocket.instances[0];
      // An attempt hanging in CONNECTING is closed by its own timeout so
      // the retry loop keeps moving on a dead link.
      vi.advanceTimersByTime(CONNECT_TIMEOUT_MS);
      expect(ws.closed).toBe(true);
      // The browser fires close after the killed attempt; the mock doesn't.
      ws.onclose();
      vi.advanceTimersByTime(RETRY_BASE_MS);
      expect(MockWebSocket.instances.length).toBe(2);
    } finally {
      vi.useRealTimers();
    }
  });

  test('reconnects immediately when the tab becomes visible again', () => {
    vi.useFakeTimers();
    try {
      const socket = newSocket();
      MockWebSocket.instances[0].onopen();
      MockWebSocket.instances[0].onclose();
      // A backgrounded tab: the browser killed the socket and froze the
      // pending retry timer. Coming back visible reconnects at once,
      // without waiting out the backoff.
      visible();
      MockWebSocket.instances[0].readyState = MockWebSocket.CLOSED;
      document.dispatchEvent(new Event('visibilitychange'));
      expect(MockWebSocket.instances.length).toBe(2);
      MockWebSocket.instances[1].onopen();
      expect(socket.onopen).toHaveBeenCalledTimes(2);
    } finally {
      vi.useRealTimers();
    }
  });

  test('leaves a healthy socket alone on visibility', () => {
    const socket = newSocket();
    MockWebSocket.instances[0].onopen();
    MockWebSocket.instances[0].readyState = MockWebSocket.OPEN;
    visible();
    document.dispatchEvent(new Event('visibilitychange'));
    expect(MockWebSocket.instances.length).toBe(1);
    expect(socket.onopen).toHaveBeenCalledTimes(1);
  });

  test('a stale close event from a replaced socket is suppressed', () => {
    vi.useFakeTimers();
    try {
      const socket = newSocket();
      MockWebSocket.instances[0].onopen();
      MockWebSocket.instances[0].onclose();
      vi.advanceTimersByTime(RETRY_BASE_MS);
      // A background tab's killed socket can deliver its close event after
      // the replacement already exists — it must not reach the consumer or
      // schedule a second retry chain.
      MockWebSocket.instances[1].onopen();
      MockWebSocket.instances[0].onclose();
      expect(socket.onclose).toHaveBeenCalledTimes(1);
      expect(MockWebSocket.instances.length).toBe(2);
    } finally {
      vi.useRealTimers();
    }
  });

  test('close() is a permanent shutdown: no retries, no events', () => {
    vi.useFakeTimers();
    try {
      const socket = newSocket();
      MockWebSocket.instances[0].onclose();
      socket.close();
      vi.advanceTimersByTime(60000);
      expect(MockWebSocket.instances.length).toBe(1);
      expect(socket.onclose).toHaveBeenCalledTimes(1);
    } finally {
      vi.useRealTimers();
    }
  });

  test('forwards onerror but leaves the state to onclose', () => {
    const socket = newSocket();
    const err = new Error('boom');
    // Browsers always follow onerror with onclose; the class relies on
    // that instead of duplicating handling on both events.
    MockWebSocket.instances[0].onerror(err);
    expect(socket.onerror).toHaveBeenCalledWith(err);
  });
});
