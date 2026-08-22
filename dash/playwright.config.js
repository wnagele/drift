// Playwright config for the dashboard E2E tests. The production bundle
// (npm run webpack) must exist before starting; the page-server serves it
// with the API/WebSocket traffic intercepted per test.
const { defineConfig, devices } = require('@playwright/test');

module.exports = defineConfig({
  testDir: './e2e',
  fullyParallel: false,
  reporter: process.env.CI ? 'github' : 'list',
  use: {
    baseURL: 'http://127.0.0.1:8321',
  },
  projects: [
    { name: 'chromium', use: { ...devices['Desktop Chrome'] } },
  ],
  webServer: {
    command: 'node e2e/page-server.js',
    url: 'http://127.0.0.1:8321',
    reuseExistingServer: !process.env.CI,
    timeout: 30000,
  },
});
