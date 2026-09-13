// Dashboard E2E against the production bundle. HTTP and WebSocket traffic
// are intercepted with the recorded device fixtures (same shared fixtures
// the firmware native tests use).
const { test, expect } = require('@playwright/test');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');

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
const statusFixture = fixture_read('api/status.json');
const broadcastFixture = fixture_read('api/broadcast.json');
const debugInfoFixture = fixture_read('api/debug-info.json');

test.beforeEach(async ({ page }) => {
  await page.route('**/debug/info', (route) => route.fulfill({ json: debugInfoFixture }));
  await page.route('**/api/config', (route) => {
    if (route.request().method() === 'GET')
      return route.fulfill({ json: configFixture });
    return route.fulfill({ status: 200 });
  });
});

test('sidebar health block and status view reflect websocket status messages', async ({ page }) => {
  await page.routeWebSocket('**/ws', (ws) => {
    // Push the recorded device messages once the app connects — both of
    // them, like the firmware's one 1 Hz task does.
    setTimeout(() => {
      ws.send(JSON.stringify(statusFixture));
      ws.send(JSON.stringify(broadcastFixture));
    }, 100);
  });

  await page.goto('/');
  // The sidebar carries all three indicators, so every box has to be located
  // by its own label rather than by a bare class.
  const health = page.locator('.status-box');
  const box = (label) => health.filter({ has: page.getByText(label, { exact: true }) });
  await expect(health).toHaveCount(3);

  // Connection: green while the stream flows, with the age of the most
  // recent update.
  const connection = box('Connected');
  await expect(connection).toHaveClass(/status-box-ok/);
  await expect(connection.locator('.status-box-detail')).toContainText(/updated \d+s ago/);

  // The fixture is asymmetric (telemetry up, gnss down), so a telemetry/gnss
  // swap anywhere fails loudly instead of cancelling out.
  const telemetry = box('Telemetry');
  const gnss = box('GNSS');
  await expect(telemetry).toHaveClass(/status-box-ok/);
  await expect(telemetry.locator('.status-dot-ok')).toBeVisible();
  await expect(gnss).toHaveClass(/status-box-down/);
  await expect(gnss.locator('.status-dot-down')).toBeVisible();
  // A failed device flag is not a link failure — the connection box stays green.
  await expect(connection).not.toHaveClass(/status-box-degraded/);

  // The Status tab (the default) carries the per-transport transmit-rate
  // table, fed by the same websocket stream (txcount.cpp diagnostics).
  const rates = page.locator('.ant-table').first();
  await expect(rates).toContainText('Bluetooth 4 legacy');
  await expect(rates).toContainText('Bluetooth 5 Long Range');
  await expect(rates).toContainText('Wi-Fi Beacon');
  await expect(rates).toContainText('Wi-Fi NAN');
  await expect(rates).toContainText('10');

  // …and below it the broadcast inspector, fed by the second message type on
  // the same socket. The enum values arrive as words because the firmware
  // interprets them, so the dash holds no ordinal table.
  const content = page.locator('.ant-table').nth(1);
  await expect(content).toContainText(broadcastFixture.uas_id + ' (Type: Serial Number)');
  await expect(content).toContainText('Airborne');
  await expect(content).toContainText('Latitude 47.3566123°, Longitude 8.5321456°, Altitude 500.5 m');
  await expect(content).toContainText('Horizontal Unknown, Vertical Unknown, Speed Unknown');
  await expect(content).toContainText('30.5 m (Type: Above Take-off)');
  await expect(content).toContainText('3.5 m/s');
  await expect(content).toContainText(broadcastFixture.operator_id);
  // The fixture configures no Self-ID, so that message is not on air and the
  // table - a list of what goes out - does not mention it. Nor does it carry
  // the ODID message names: which message a field rides in is the firmware's
  // business.
  await expect(content).not.toContainText('Description');
  await expect(content).not.toContainText('Basic ID');
  await expect(content).not.toContainText('Self-ID');

  // The health block survives a tab switch, which is the whole point of it
  // living in the sider.
  await page.getByRole('menuitem', { name: 'Config' }).click();
  await expect(health).toHaveCount(3);
  await expect(box('Connected')).toHaveClass(/status-box-ok/);
});

test('a narrow viewport collapses the health block to icons and dots', async ({ page }) => {
  // antd's Sider breakpoint="md" (768px) is a real-browser media query, so
  // this collapse cannot be exercised in jsdom — it only exists here.
  await page.routeWebSocket('**/ws', (ws) => {
    setTimeout(() => ws.send(JSON.stringify(statusFixture)), 100);
  });

  await page.setViewportSize({ width: 600, height: 800 });
  await page.goto('/');

  // No boxes at all, and no label text: the sider is too narrow for either.
  await expect(page.locator('.status-box')).toHaveCount(0);
  const items = page.locator('.status-box-collapsed');
  await expect(items).toHaveCount(3);

  // Each item keeps an identity icon, so the block is readable without
  // hovering — three bare dots would be all colour and no identity.
  await expect(items.nth(0).locator('.anticon-api')).toBeVisible();
  await expect(items.nth(1).locator('.anticon-dashboard')).toBeVisible();
  await expect(items.nth(2).locator('.anticon-environment')).toBeVisible();

  // …and the asymmetric fixture still shows through the dots beside them.
  await expect(items.nth(0).locator('.status-dot-ok')).toBeVisible();
  await expect(items.nth(1).locator('.status-dot-ok')).toBeVisible();
  await expect(items.nth(2).locator('.status-dot-down')).toBeVisible();

  // The tooltip is the only place the names survive, so it has to be there.
  await expect(items.nth(1)).toHaveAttribute('title', 'Telemetry');
  await expect(items.nth(2)).toHaveAttribute('title', 'GNSS');
  await expect(items.nth(0)).toHaveAttribute('title', /^Connected · updated \d+s ago$/);

  // Widening it again brings the full boxes back.
  await page.setViewportSize({ width: 1200, height: 800 });
  await expect(page.locator('.status-box')).toHaveCount(3);
  await expect(page.locator('.status-box-collapsed')).toHaveCount(0);
});

test('connection box flags a silent websocket as no-data', async ({ page }) => {
  // The mocked socket accepts the connection but never delivers a status
  // message: the box must show No data right away (no green grace period —
  // no message yet IS no data) and stay there.
  await page.routeWebSocket('**/ws', (ws) => {});

  await page.goto('/');
  const box = page.locator('.status-box')
    .filter({ has: page.getByText('No data', { exact: true }) });
  await expect(box).toContainText('No data');
  await expect(box).toHaveClass(/status-box-degraded/);
  await expect(box.locator('.status-box-detail')).toContainText('no message yet');
  await expect(box.locator('.status-dot-ok')).toHaveCount(0);
  // And it stays no-data — nothing arrived to clear it.
  await page.waitForTimeout(3000);
  await expect(box).toContainText('No data');
  await expect(box).toHaveClass(/status-box-degraded/);
});

test('connection box shows disconnection with its age', async ({ page }) => {
  // The mocked socket closes right after connecting: red box, Disconnected,
  // and the detail line reporting for how long.
  await page.routeWebSocket('**/ws', (ws) => {
    setTimeout(() => ws.close(), 100);
  });

  await page.goto('/');
  const box = page.locator('.status-box')
    .filter({ has: page.getByText('Disconnected', { exact: true }) });
  await expect(box).toContainText('Disconnected');
  await expect(box).toHaveClass(/status-box-down/);
  await expect(box.locator('.status-box-detail')).toContainText(/since \d+s ago/);
});

test('config save flow posts the edited config to the API', async ({ page }) => {
  await page.goto('/');

  // Footer falls back through debug info when nothing is provided.
  await expect(page.getByText(/Build Info: UNKNOWN/)).toBeVisible();

  await page.getByRole('menuitem', { name: /config/i }).click();
  await expect(page.getByLabel('WiFi SSID')).toHaveValue(configFixture.wifi.ssid);
  // The fixture is an EU device, so the operator field carries EASA's own
  // label and shows the two stored halves rejoined into one value.
  await expect(page.getByLabel('Operator Registration Number'))
    .toHaveValue(`${configFixture.dri.op_id}-${configFixture.dri.op_secret}`);
  // Both verification indicators read green for the fixture, whose checksum
  // character is a real Luhn mod-36 over its random part plus secret digits.
  await expect(page.getByText(/Syntax check passed/)).toBeVisible();
  await expect(page.getByText(/Security check passed/)).toBeVisible();

  let postBody = null;
  await page.route('**/api/config', (route) => {
    if (route.request().method() === 'POST') {
      try {
        postBody = route.request().postDataJSON();
      } catch {
        // empty body
      }
    }
    return route.fulfill({ status: 200 });
  });

  // Paste the full 20-character registration string, separators and all:
  // the dash must split it so only the public half is posted as the
  // broadcast value and the private key is stored apart.
  await page.getByLabel('Operator Registration Number').fill('NOR drift0test01 h - abc');
  await page.getByRole('button', { name: 'Save' }).click();

  await expect(page.getByText('Config saved.')).toBeVisible();
  expect(postBody).not.toBeNull();
  expect(postBody.dri.op_id).toEqual('NORdrift0test01h');
  expect(postBody.dri.op_secret).toEqual('abc');
  expect(postBody.wifi.ssid).toEqual(configFixture.wifi.ssid);
  expect(postBody.dri.bt5_enabled).toEqual(configFixture.dri.bt5_enabled);
  expect(postBody.dri.wifi_beacon_enabled).toEqual(configFixture.dri.wifi_beacon_enabled);
  expect(postBody.dri.wifi_nan_enabled).toEqual(configFixture.dri.wifi_nan_enabled);
});

test('config save flow posts the BT5 transport state', async ({ page }) => {
  await page.goto('/');

  await page.getByRole('menuitem', { name: /config/i }).click();
  const toggle = page.getByRole('switch', { name: /bluetooth 5 long range/i });
  await expect(toggle).toBeVisible();
  if (configFixture.dri.bt5_enabled)
    await expect(toggle).toBeChecked();
  else
    await expect(toggle).not.toBeChecked();

  let postBody = null;
  await page.route('**/api/config', (route) => {
    if (route.request().method() === 'POST') {
      try {
        postBody = route.request().postDataJSON();
      } catch {
        // empty body
      }
    }
    return route.fulfill({ status: 200 });
  });

  await toggle.click();
  await page.getByRole('button', { name: 'Save' }).click();

  await expect(page.getByText('Config saved.')).toBeVisible();
  expect(postBody).not.toBeNull();
  expect(postBody.dri.bt5_enabled).toEqual(!configFixture.dri.bt5_enabled);
});

test('config save flow posts the Wi-Fi Beacon transport state', async ({ page }) => {
  await page.goto('/');

  await page.getByRole('menuitem', { name: /config/i }).click();
  const toggle = page.getByRole('switch', { name: /wi-fi beacon/i });
  await expect(toggle).toBeVisible();
  if (configFixture.dri.wifi_beacon_enabled)
    await expect(toggle).toBeChecked();
  else
    await expect(toggle).not.toBeChecked();

  let postBody = null;
  await page.route('**/api/config', (route) => {
    if (route.request().method() === 'POST') {
      try {
        postBody = route.request().postDataJSON();
      } catch {
        // empty body
      }
    }
    return route.fulfill({ status: 200 });
  });

  await toggle.click();
  await page.getByRole('button', { name: 'Save' }).click();

  await expect(page.getByText('Config saved.')).toBeVisible();
  expect(postBody).not.toBeNull();
  expect(postBody.dri.wifi_beacon_enabled).toEqual(!configFixture.dri.wifi_beacon_enabled);
});

test('config save flow posts the Wi-Fi NAN transport state', async ({ page }) => {
  await page.goto('/');

  await page.getByRole('menuitem', { name: /config/i }).click();
  const toggle = page.getByRole('switch', { name: /wi-fi nan/i });
  await expect(toggle).toBeVisible();
  if (configFixture.dri.wifi_nan_enabled)
    await expect(toggle).toBeChecked();
  else
    await expect(toggle).not.toBeChecked();

  let postBody = null;
  await page.route('**/api/config', (route) => {
    if (route.request().method() === 'POST') {
      try {
        postBody = route.request().postDataJSON();
      } catch {
        // empty body
      }
    }
    return route.fulfill({ status: 200 });
  });

  await toggle.click();
  await page.getByRole('button', { name: 'Save' }).click();

  await expect(page.getByText('Config saved.')).toBeVisible();
  expect(postBody).not.toBeNull();
  expect(postBody.dri.wifi_nan_enabled).toEqual(!configFixture.dri.wifi_nan_enabled);
});
