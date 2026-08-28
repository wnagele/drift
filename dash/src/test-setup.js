import '@testing-library/jest-dom/vitest';
import { cleanup } from '@testing-library/react';
import { message } from 'antd';
import { afterEach, afterAll } from 'vitest';

afterEach(() => {
  cleanup();
  // antd's imperative message toasts render into a detached React root that
  // RTL's cleanup never unmounts, and each schedules a ~3 s auto-close
  // timer. Destroy them per test (and once more before the environment goes
  // away) so no timer can fire after jsdom teardown — a surviving one calls
  // setState on the dead root and Vitest fails the whole run on the
  // resulting unhandled "window is not defined".
  message.destroy();
});

afterAll(() => message.destroy());

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
