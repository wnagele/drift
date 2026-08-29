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
const debugInfoFixture = fixture_read('api/debug-info.json');

test.beforeEach(async ({ page }) => {
  await page.route('**/debug/info', (route) => route.fulfill({ json: debugInfoFixture }));
  await page.route('**/api/config', (route) => {
    if (route.request().method() === 'GET')
      return route.fulfill({ json: configFixture });
    return route.fulfill({ status: 200 });
  });
});

test('connection box and status view reflect websocket status messages', async ({ page }) => {
  await page.routeWebSocket('**/ws', (ws) => {
    // Push the recorded device status once the app connects.
    setTimeout(() => ws.send(JSON.stringify(statusFixture)), 100);
  });

  await page.goto('/');
  // Sidebar connection box: green while the stream flows, with the age of
  // the most recent update.
  const box = page.locator('.status-box');
  await expect(box).toHaveClass(/status-box-ok/);
  await expect(box).toContainText('Connected');
  await expect(box.locator('.status-box-detail')).toContainText(/updated \d+s ago/);
  // Status view (default tab) carries the flags only — no connection row.
  const telemetryRow = page.locator('.ant-row', { hasText: 'Telemetry' });
  const gnssRow = page.locator('.ant-row', { hasText: 'GNSS' });
  await expect(page.locator('.ant-row', { hasText: 'Connection' })).toHaveCount(0);
  // The fixture is asymmetric (telemetry up, gnss down), so a telemetry/gnss
  // swap anywhere fails loudly instead of cancelling out.
  await expect(telemetryRow.locator('.anticon-check-circle')).toBeVisible();
  await expect(telemetryRow.locator('.anticon-close-circle')).toHaveCount(0);
  await expect(gnssRow.locator('.anticon-close-circle')).toBeVisible();
  await expect(gnssRow.locator('.anticon-check-circle')).toHaveCount(0);
  // GNSS being down degrades the flags, not the connection box.
  await expect(box).not.toHaveClass(/status-box-degraded/);

  // The Statistics tab carries the per-transport transmit-rate table, fed by
  // the same websocket stream (txcount.cpp diagnostics).
  await page.getByRole('menuitem', { name: 'Statistics' }).click();
  const rates = page.locator('.ant-table');
  await expect(rates).toContainText('Bluetooth 4 legacy');
  await expect(rates).toContainText('Bluetooth 5 Long Range');
  await expect(rates).toContainText('Wi-Fi Beacon');
  await expect(rates).toContainText('Wi-Fi NAN');
  await expect(rates).toContainText('10');
});

test('connection box flags a silent websocket as no-data', async ({ page }) => {
  // The mocked socket accepts the connection but never delivers a status
  // message: the box must show No data right away (no green grace period —
  // no message yet IS no data) and stay there.
  await page.routeWebSocket('**/ws', (ws) => {});

  await page.goto('/');
  const box = page.locator('.status-box');
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
  const box = page.locator('.status-box');
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
  await expect(page.getByLabel('Operator ID')).toHaveValue(configFixture.dri.op_id);

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

  await page.getByLabel('Operator ID').fill('E2E-OPERATOR');
  await page.getByRole('button', { name: 'Save' }).click();

  await expect(page.getByText('Config saved.')).toBeVisible();
  expect(postBody).not.toBeNull();
  expect(postBody.dri.op_id).toEqual('E2E-OPERATOR');
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
