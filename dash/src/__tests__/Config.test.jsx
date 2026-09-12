import React from 'react';
import { render, screen, fireEvent, waitFor, within } from '@testing-library/react';
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

// The dash shows the two stored halves as one field, in EASA's rendering.
const OPERATOR_FULL = `${fixture.dri.op_id}-${fixture.dri.op_secret}`;
// A valid number for each region, for the tests that switch region: the
// fixture's AUT number is invalid under UK, which requires a GBR prefix.
const VALID_BY_REGION = { EU: OPERATOR_FULL, UK: 'GBRgc284pmztcrth-abc' };

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

// Pick a region from the Select. Scoped to the dropdown portal because the
// currently selected region renders its label a second time in the closed
// control, so a bare getByText is ambiguous for whichever region is active.
async function selectRegion(label) {
  fireEvent.mouseDown(screen.getByLabelText(/region/i));
  const dropdown = await waitFor(() => {
    const node = document.querySelector('.ant-select-dropdown');
    expect(node).not.toBeNull();
    return node;
  });
  fireEvent.click(within(dropdown).getByText(label));
}

describe('Config', () => {
  test('loads and displays the current config from the API', async () => {
    render(<Config />);
    // The shared API fixture (also used by firmware native tests).
    await screen.findByDisplayValue(fixture.wifi.ssid);
    expect(screen.getByDisplayValue(fixture.dri.ua_id)).toBeInTheDocument();
    expect(screen.getByDisplayValue(OPERATOR_FULL)).toBeInTheDocument();
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
    const toggle = await screen.findByRole('switch', { name: /bluetooth 5 long range/i });
    expect(toggle).toBeChecked();
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.bt5_enabled).toEqual(true);

    // Toggling it off is part of the posted wire format.
    fireEvent.click(toggle);
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted.dri.bt5_enabled).toEqual(false));
  });

  test('loads and submits the Wi-Fi Beacon transport state', async () => {
    const { container } = render(<Config />);
    // The shared fixture has the transport on.
    const toggle = await screen.findByRole('switch', { name: /wi-fi beacon/i });
    expect(toggle).toBeChecked();
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.wifi_beacon_enabled).toEqual(true);

    // Toggling it off is part of the posted wire format.
    fireEvent.click(toggle);
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted.dri.wifi_beacon_enabled).toEqual(false));
  });

  test('loads and submits the Wi-Fi NAN transport state', async () => {
    const { container } = render(<Config />);
    // The shared fixture has the opt-in transport off.
    const toggle = await screen.findByRole('switch', { name: /wi-fi nan/i });
    expect(toggle).not.toBeChecked();
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.wifi_nan_enabled).toEqual(false);

    // Toggling it on is part of the posted wire format.
    fireEvent.click(toggle);
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted.dri.wifi_nan_enabled).toEqual(true));
  });

  test('posts changed values', async () => {
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    // The fixture is an EU device, so the field carries EASA's label.
    fireEvent.change(screen.getByLabelText(/operator registration number/i),
      { target: { value: 'DEUabcdefghijkl9' } });
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.op_id).toEqual('DEUabcdefghijkl9');
  });

  test('loads the configured region and posts it back', async () => {
    const { container } = render(<Config />);
    // The shared fixture is an EU device; the Select renders the human label
    // while the posted document carries the code the firmware allow-lists.
    expect(await screen.findByTitle('European Union')).toBeInTheDocument();
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.region).toEqual('EU');
  });

  test('every offered region is selectable and posts the firmware code', async () => {
    // config.cpp's allow-list is US/EU/UK. This pins the label -> code
    // mapping rather than the dropdown's contents: a label wired to the
    // wrong code (say "United Kingdom" -> "GB") is a guaranteed 400 on save,
    // and that is invisible to a test that only checks the options exist.
    // Asserted by selecting each one, which also sidesteps antd's virtual
    // list rendering only part of the dropdown under jsdom's zero heights.
    for (const [label, code] of [['United States', 'US'], ['European Union', 'EU'],
                                 ['United Kingdom', 'UK'],
                                 ['Other — no regional rules', '']]) {
      posted = null;
      const { container, unmount } = render(<Config />);
      await screen.findByDisplayValue(fixture.wifi.ssid);
      await selectRegion(label);
      // Switching region changes what a valid operator number looks like,
      // and the syntax check blocks saving, so set a region-valid one. US
      // hides the field entirely.
      if (VALID_BY_REGION[code]) {
        fireEvent.change(screen.getByLabelText(/operator registration number|remote id number/i),
          { target: { value: VALID_BY_REGION[code] } });
      }
      fireEvent.submit(container.querySelector('form'));
      await waitFor(() => expect(posted).not.toBeNull());
      expect(posted.dri.region, label).toEqual(code);
      unmount();
    }
  });

  test('a device reporting no region loads as "Other" and saves', async () => {
    // The firmware defaults dri_region to "", and "" is also what the
    // "Other — no regional rules" choice posts, so the two are the same
    // value. That is a deliberate trade-off: a factory-fresh device loads
    // showing "Other" instead of prompting for a region, so the user is
    // never forced to confront the question. The upside is that "Other" is
    // the correct answer for every jurisdiction DRIFT does not model
    // (Australia and Canada mandate nothing at all) and an escape hatch if
    // the EU/UK syntax check rejects a number it should not.
    server.use(http.get('/api/config', () =>
      HttpResponse.json({ ...fixture, dri: { ...fixture.dri, region: '' } })));
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    expect(await screen.findByTitle('Other — no regional rules')).toBeInTheDocument();

    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.region).toEqual('');
  });

  test('"Other" leaves the operator ID opaque: no rules, no split', async () => {
    // No registry is being modelled, so there are no secret digits to split
    // off. Splitting anyway would silently move the tail of a plain
    // 20-character ID into the verification code.
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    await selectRegion('Other — no regional rules');

    const field = screen.getByLabelText(/operator id/i);
    // Capped at ODID_ID_SIZE, which is what the firmware will actually
    // broadcast, rather than the wider paste-friendly cap EU/UK needs.
    expect(field).toHaveAttribute('maxlength', '20');

    fireEvent.change(field, { target: { value: 'WHATEVER-FORMAT-01' } });
    // A value that no region would accept: no indicators, no blocking.
    expect(screen.queryByText(/syntax check/i)).toBeNull();
    expect(screen.queryByText(/security check/i)).toBeNull();

    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.op_id).toEqual('WHATEVER-FORMAT-01');
    expect(posted.dri.op_secret).toEqual('');
  });

  test('"Other" does not require an operator ID', async () => {
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    await selectRegion('Other — no regional rules');
    fireEvent.change(screen.getByLabelText(/operator id/i), { target: { value: '' } });
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.op_id).toEqual('');
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
    // The operator field accepts a pasted full registration string plus
    // separators, so it is deliberately not capped at ODID_ID_SIZE.
    expect(screen.getByLabelText(/operator registration number/i)).toHaveAttribute('maxlength', '32');
    expect(screen.getByLabelText(/unmanned aircraft description/i)).toHaveAttribute('maxlength', '23');
    // And a full-length UA ID from the fixture survives the round trip.
    expect(screen.getByLabelText(/unmanned aircraft id/i)).toHaveValue(fixture.dri.ua_id);
  });

  test('hides the operator field under US, and clears both stored halves', async () => {
    // 14 CFR 89.315 lists no operator-registration field for a broadcast
    // module, so there is nothing to ask for. Clearing it also leaves
    // OperatorID invalid in the firmware, so nothing is broadcast.
    const { container } = render(<Config />);
    await screen.findByDisplayValue(OPERATOR_FULL);
    await selectRegion('United States');
    expect(screen.queryByLabelText(/operator registration number/i)).toBeNull();
    expect(screen.queryByLabelText(/remote id number/i)).toBeNull();

    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.op_id).toEqual('');
    expect(posted.dri.op_secret).toEqual('');
  });

  test('uses each authority own name for the operator field', async () => {
    render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    // EASA calls it the operator registration number; the CAA calls it the
    // Remote ID number. Neither is "Operator ID".
    expect(screen.getByLabelText(/operator registration number/i)).toBeInTheDocument();
    await selectRegion('United Kingdom');
    expect(screen.getByLabelText(/remote id number/i)).toBeInTheDocument();
  });

  test('splits a pasted full registration string into its public and secret halves', async () => {
    // The trap this closes: EASA's 20-character full registration string is
    // exactly ODID_ID_SIZE, so pasting it into a 20-char field used to fit
    // perfectly - and would have broadcast the private key, which the CAA
    // explicitly warns against.
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.wifi.ssid);
    fireEvent.change(screen.getByLabelText(/operator registration number/i),
      { target: { value: 'AUT drift0test01 h - abc' } });
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    // Separators in either official rendering are sanitized away, and only
    // the 16-character public half is the broadcast value.
    expect(posted.dri.op_id).toEqual('AUTdrift0test01h');
    expect(posted.dri.op_secret).toEqual('abc');
  });

  test('reports both checks, and blocks saving on a syntax failure', async () => {
    const { container } = render(<Config />);
    await screen.findByDisplayValue(OPERATOR_FULL);
    // The fixture's number carries a real Luhn mod-36 checksum over its
    // random part plus its secret digits, so both checks pass.
    expect(await screen.findByText(/syntax check passed/i)).toBeInTheDocument();
    expect(await screen.findByText(/security check passed/i)).toBeInTheDocument();

    fireEvent.change(screen.getByLabelText(/operator registration number/i),
      { target: { value: 'XXXdrift0test01h-abc' } });
    fireEvent.submit(container.querySelector('form'));
    expect(await screen.findByText(/is not an EASA member state country code/i)).toBeInTheDocument();
    expect(posted).toBeNull();
  });

  test('a missing verification code is neutral, not a failure', async () => {
    // Most national registries appear not to issue the secret digits at all
    // (Austria issues only the 16-character number), so the majority of
    // compliant operators have none. That must not render as an error.
    server.use(http.get('/api/config', () =>
      HttpResponse.json({ ...fixture, dri: { ...fixture.dri, op_secret: '' } })));
    const { container } = render(<Config />);
    await screen.findByDisplayValue(fixture.dri.op_id);
    expect(await screen.findByText(/syntax check passed/i)).toBeInTheDocument();
    expect(await screen.findByText(/security check not available/i)).toBeInTheDocument();
    expect(screen.queryByText(/security check failed/i)).toBeNull();

    // And it still saves.
    fireEvent.submit(container.querySelector('form'));
    await waitFor(() => expect(posted).not.toBeNull());
    expect(posted.dri.op_secret).toEqual('');
  });

  test('a mismatched verification code fails the security check', async () => {
    render(<Config />);
    await screen.findByDisplayValue(OPERATOR_FULL);
    fireEvent.change(screen.getByLabelText(/operator registration number/i),
      { target: { value: 'AUTdrift0test01h-zzz' } });
    // Structure is still fine - only the checksum disagrees.
    expect(await screen.findByText(/syntax check passed/i)).toBeInTheDocument();
    expect(await screen.findByText(/security check failed/i)).toBeInTheDocument();
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
