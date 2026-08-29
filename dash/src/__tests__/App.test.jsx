import React from 'react';
import { render, act, fireEvent, screen } from '@testing-library/react';
import { setupServer } from 'msw/node';
import { http, HttpResponse } from 'msw';
import { beforeAll, afterEach, afterAll, beforeEach, describe, test, expect } from 'vitest';

import App from '../App.js';
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

const configFixture = fixture_read('api/config.json');

// App owns one /ws connection via useStatusSocket (sidebar connection box +
// Status view), regardless of the selected tab.
class MockWebSocket {
  static instances = [];
  constructor(url) {
    this.url = url;
    MockWebSocket.instances.push(this);
  }
  close() {}
}

const debugFixture = { version: null, git_ref: null, build_time: null };

let debugRequests = 0;
const server = setupServer(
  http.get('/debug/info', () => {
    debugRequests++;
    return HttpResponse.json(debugFixture);
  }),
  // Switching to the Config tab loads the config form — handle it so the
  // request can't fail against the real network and pop a stray error
  // toast after the test ends.
  http.get('/api/config', () => HttpResponse.json(configFixture)),
);

beforeAll(() => server.listen());
beforeEach(() => {
  debugRequests = 0;
  MockWebSocket.instances = [];
  Object.defineProperty(global, 'WebSocket', { writable: true, value: MockWebSocket });
});
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

// The footer's build-info precedence: version > git_ref (first 7 chars) >
// build_time > UNKNOWN.
function footer() {
  return screen.getByText(/^Build Info: /).textContent;
}

describe('App build info precedence', () => {
  test('shows the version when provided', async () => {
    debugFixture.version = 'v1.2.3';
    debugFixture.git_ref = 'abcdef1234567890';
    debugFixture.build_time = '2026-01-01T00:00:00Z';
    render(<App />);
    await screen.findByText(/^Build Info: /, {}, { timeout: 2000 });
    expect(footer()).toBe('Build Info: v1.2.3');
  });

  test('falls back to the first 7 chars of the git ref', async () => {
    debugFixture.version = null;
    debugFixture.git_ref = 'abcdef1234567890';
    debugFixture.build_time = '2026-01-01T00:00:00Z';
    render(<App />);
    await screen.findByText('Build Info: abcdef1', {}, { timeout: 2000 });
  });

  test('falls back to the build time without a git ref', async () => {
    debugFixture.version = null;
    debugFixture.git_ref = null;
    debugFixture.build_time = '2026-01-01T00:00:00Z';
    render(<App />);
    await screen.findByText('Build Info: 2026-01-01T00:00:00Z', {}, { timeout: 2000 });
  });

  test('shows UNKNOWN when nothing is provided', async () => {
    debugFixture.version = null;
    debugFixture.git_ref = null;
    debugFixture.build_time = null;
    render(<App />);
    await screen.findByText('Build Info: UNKNOWN', {}, { timeout: 2000 });
  });

  test('fetches /debug/info exactly once, also across tab switches', async () => {
    render(<App />);
    await screen.findByText('Build Info: UNKNOWN', {}, { timeout: 2000 });
    expect(debugRequests).toBe(1);

    act(() => {
      fireEvent.click(screen.getByRole('menuitem', { name: /config/i }));
    });
    act(() => {
      fireEvent.click(screen.getByRole('menuitem', { name: /status/i }));
    });
    expect(debugRequests).toBe(1);
  });

  test('shows the connection box in the sidebar and one shared websocket', async () => {
    const { container } = render(<App />);
    await screen.findByText('Build Info: UNKNOWN', {}, { timeout: 2000 });

    // The connection box lives in the sider, visible from every tab…
    expect(container.querySelector('.status-box')).not.toBeNull();
    // …and all three tabs exist, with exactly one websocket for the whole app.
    expect(screen.queryByRole('menuitem', { name: /status/i })).not.toBeNull();
    expect(screen.queryByRole('menuitem', { name: /statistics/i })).not.toBeNull();
    expect(screen.queryByRole('menuitem', { name: /config/i })).not.toBeNull();
    expect(MockWebSocket.instances).toHaveLength(1);
    expect(MockWebSocket.instances[0].url).toBe('ws://' + window.location.host + '/ws');

    act(() => {
      fireEvent.click(screen.getByRole('menuitem', { name: /statistics/i }));
    });
    expect(MockWebSocket.instances).toHaveLength(1);
    act(() => {
      fireEvent.click(screen.getByRole('menuitem', { name: /config/i }));
    });
    expect(MockWebSocket.instances).toHaveLength(1);
  });
});
