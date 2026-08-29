import '@testing-library/jest-dom/vitest';
import { cleanup } from '@testing-library/react';
import { message } from 'antd';
import { afterEach, afterAll } from 'vitest';

// antd's imperative message toasts render into a detached React root that
// RTL's cleanup never unmounts, and each schedules a ~3 s auto-close
// timer. Destroy them per test (and once more before the environment goes
// away) so no timer can fire after jsdom teardown — a surviving one calls
// setState on the dead root and Vitest fails the whole run on the
// resulting unhandled "window is not defined".
//
// The destroy must follow a macrotask yield: a toast's mount is scheduled
// on React's concurrent scheduler and only lands after two macrotask
// rounds (probe-verified), so a destroy() called synchronously right
// after the test finds nothing to destroy — the toast then mounts, its
// 3 s timer survives every later sweep, and fires into the torn-down
// environment (CI once failed the whole run on it, attributed to whatever
// test file was running three seconds later). Yielding first lets the
// toast mount so the destroy actually reaches it.
const settle = async () => {
  for (let i = 0; i < 4; i++)
    await new Promise((resolve) => setTimeout(resolve, 0));
};

afterEach(async () => {
  cleanup();
  await settle();
  message.destroy();
});

afterAll(async () => {
  await settle();
  message.destroy();
});

// antd's responsive grid hooks expect matchMedia, which jsdom lacks.
Object.defineProperty(window, 'matchMedia', {
  writable: true,
  value: (query) => ({
    matches: false,
    media: query,
    onchange: null,
    addListener: () => {},
    removeListener: () => {},
    addEventListener: () => {},
    removeEventListener: () => {},
    dispatchEvent: () => false,
  }),
});
