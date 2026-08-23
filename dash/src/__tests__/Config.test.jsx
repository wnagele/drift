import React from 'react';
import { render, screen, fireEvent, waitFor } from '@testing-library/react';
import { setupServer } from 'msw/node';
import { http, HttpResponse } from 'msw';
import { beforeAll, afterEach, afterAll, describe, test, expect } from 'vitest';

import Config from '../Config.js';

// Shared API fixture, read from the repo root the same way the firmware
// native tests do it (works from dash/ or repo-root working directories).
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

const fixture = fixture_read('api/config.json');

let posted = null;
const server = setupServer(
  http.get('/api/config', () => HttpResponse.json(fixture)),
  http.post('/api/config', async ({ request }) => {
    posted = await request.json();
    return new HttpResponse(null, { status: 200 });
  }),
);

beforeAll(() => server.listen());
afterEach(() => {
  server.resetHandlers();
  posted = null;
});
afterAll(() => server.close());

describe('Config', () => {
  test('loads and displays the current config from the API', async () => {
    render(<Config />);
    // The shared API fixture (also used by firmware native tests).
    await screen.findByDisplayValue(fixture.wifi.ssid);
    expect(screen.getByDisplayValue(fixture.dri.ua_id)).toBeInTheDocument();
    expect(screen.getByDisplayValue(fixture.dri.op_id)).toBeInTheDocument();
    expect(screen.getByDisplayValue(fixture.dri.ua_desc)).toBeInTheDocument();
  });

  test('submits the config back to the API in the wire format', async () => {
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted).toEqual(fixture);
  });

  test('loads and submits the BT5 Long Range transport state', async () => {
    const { container } = render(<Config />);
    // The shared fixture has the transport on.
    const toggle = await screen.findByRole('switch');
    expect(toggle).toBeChecked();
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.bt5_enabled).toEqual(true);

    // Toggling it off is part of the posted wire format.
    fireEvent.click(toggle);
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted.dri.bt5_enabled).toEqual(false));
  });

  test('posts changed values', async () => {
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    fireEvent.change(screen.getByLabelText(/operator id/i), { target: { value: 'NEW-OPERATOR' } });
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.op_id).toEqual('NEW-OPERATOR');
  });

  test('blocks submission when a required field is empty', async () => {
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    fireEvent.change(screen.getByLabelText(/wifi ssid/i), { target: { value: '' } });
    fireEvent.submit(container.querySelector('form'));
    expect(await screen.findByText('WiFi SSID must be set.')).toBeInTheDocument();
    expect(posted).toBeNull();
  });

  test('rejects a password shorter than 8 characters', async () => {
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    fireEvent.change(screen.getByLabelText(/wifi password/i), { target: { value: 'short7' } });
    fireEvent.submit(container.querySelector('form'));
    expect(await screen.findByText('WiFi Password must be at least 8 characters long.')).toBeInTheDocument();
    expect(posted).toBeNull();
  });

  test('input bounds mirror the firmware ODID limits', async () => {
    // The firmware enforces ODID_ID_SIZE (20) / ODID_STR_SIZE (23) in
    // dri_populate_identity; the UI side may not drift from those bounds.
    render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    expect(screen.getByLabelText(/unmanned aircraft id/i)).toHaveAttribute('maxlength', '20');
    expect(screen.getByLabelText(/operator id/i)).toHaveAttribute('maxlength', '20');
    expect(screen.getByLabelText(/unmanned aircraft description/i)).toHaveAttribute('maxlength', '23');
    // And a full-length UA ID from the fixture survives the round trip.
    expect(screen.getByLabelText(/unmanned aircraft id/i)).toHaveValue(fixture.dri.ua_id);
  });

  test('reports a failing config load', async () => {
    server.use(http.get('/api/config', () => new HttpResponse(null, { status: 500 })));
    render(<Config />);
    expect(await screen.findByText('Could not get current values.')).toBeInTheDocument();
  });

  test('reports a failing save', async () => {
    server.use(http.post('/api/config', () => new HttpResponse(null, { status: 500 })));
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    fireEvent.submit(container.querySelector('form'));
    expect(await screen.findByText('Config failed to save.')).toBeInTheDocument();
  });
});
