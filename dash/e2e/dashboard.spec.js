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

test('status display reflects websocket status messages', async ({ page }) => {
  await page.routeWebSocket('**/ws', (ws) => {
    // Push the recorded device status once the app connects.
    setTimeout(() => ws.send(JSON.stringify(statusFixture)), 100);
  });

  await page.goto('/');
  // Per-row assertions: the fixture is asymmetric (telemetry up, gnss down),
  // so a telemetry/gnss swap anywhere fails loudly instead of cancelling out.
  const telemetryRow = page.locator('.ant-row', { hasText: 'Telemetry' });
  const gnssRow = page.locator('.ant-row', { hasText: 'GNSS' });
  await expect(telemetryRow.locator('.anticon-check-circle')).toBeVisible();
  await expect(telemetryRow.locator('.anticon-close-circle')).toHaveCount(0);
  await expect(gnssRow.locator('.anticon-close-circle')).toBeVisible();
  await expect(gnssRow.locator('.anticon-check-circle')).toHaveCount(0);
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
