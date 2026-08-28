import React from 'react';
import { render } from '@testing-library/react';
import { describe, test, expect } from 'vitest';

import ConnectionBox from '../ConnectionBox.js';

function box(container) {
  const box = container.querySelector('.status-box');
  expect(box).not.toBeNull();
  return box;
}

function container_dot(container, state) {
  expect(container.querySelector('.status-dot-' + state)).not.toBeNull();
  return true;
}

describe('ConnectionBox', () => {
  test('connecting renders the degraded box with no detail line', () => {
    const { container } = render(
      <ConnectionBox connection="connecting" stale={false} msgAgeMs={null} />
    );
    const b = box(container);
    expect(b.className).toContain('status-box-degraded');
    expect(b.textContent).toContain('Connecting…');
    expect(container.querySelector('.status-dot-degraded')).not.toBeNull();
    expect(container.querySelector('.status-box-detail')).toBeNull();
  });

  test('connected with a fresh message reports its age', () => {
    const { container } = render(
      <ConnectionBox connection="connected" stale={false} msgAgeMs={2000} />
    );
    const b = box(container);
    expect(b.className).toContain('status-box-ok');
    expect(b.textContent).toContain('Connected');
    expect(b.querySelector('.status-box-detail').textContent).toBe('updated 2s ago');
    expect(container.querySelector('.status-dot-ok')).not.toBeNull();
  });

  test('connected before the first message is no-data, not green', () => {
    const { container } = render(
      <ConnectionBox connection="connected" stale={true} msgAgeMs={null} />
    );
    const b = box(container);
    expect(b.className).toContain('status-box-degraded');
    expect(b.textContent).toContain('No data');
    expect(b.querySelector('.status-box-detail').textContent).toBe('no message yet');
  });

  test('caps negative ages at zero', () => {
    // The hook clamps, but the display must too — a `now` tick lagging a
    // just-arrived event can never render "-1s ago".
    const updated = render(
      <ConnectionBox connection="connected" stale={false} msgAgeMs={-1500} />
    );
    expect(box(updated.container).querySelector('.status-box-detail').textContent)
      .toBe('updated 0s ago');

    const stale = render(
      <ConnectionBox connection="connected" stale={true} msgAgeMs={-1500} />
    );
    expect(box(stale.container).querySelector('.status-box-detail').textContent)
      .toBe('last update 0s ago');

    const closed = render(
      <ConnectionBox connection="closed" stale={false} msgAgeMs={null} closedAgeMs={-1500} />
    );
    expect(box(closed.container).querySelector('.status-box-detail').textContent)
      .toBe('since 0s ago');
  });

  test('open but silent flips to No data with the last update age', () => {
    const withData = render(
      <ConnectionBox connection="connected" stale={true} msgAgeMs={7000} />
    );
    let b = box(withData.container);
    expect(b.className).toContain('status-box-degraded');
    expect(b.textContent).toContain('No data');
    expect(b.textContent).not.toContain('Connected'); // "No data" word only
    expect(b.querySelector('.status-box-detail').textContent).toBe('last update 7s ago');

    const never = render(
      <ConnectionBox connection="connected" stale={true} msgAgeMs={null} />
    );
    b = box(never.container);
    expect(b.querySelector('.status-box-detail').textContent).toBe('no message yet');
  });

  test('closed renders the red box with the disconnection age', () => {
    const withAge = render(
      <ConnectionBox connection="closed" stale={false} msgAgeMs={1000} closedAgeMs={12000} />
    );
    let b = box(withAge.container);
    expect(b.className).toContain('status-box-down');
    expect(b.textContent).toContain('Disconnected');
    expect(container_dot(withAge.container, 'down'));
    expect(b.querySelector('.status-box-detail').textContent).toBe('since 12s ago');

    // Without a known close time (should not happen via the hook), no detail.
    const noAge = render(
      <ConnectionBox connection="closed" stale={false} msgAgeMs={null} closedAgeMs={null} />
    );
    b = box(noAge.container);
    expect(b.querySelector('.status-box-detail')).toBeNull();
  });

  test('collapsed sider renders a dot with a tooltip summary', () => {
    const { container } = render(
      <ConnectionBox connection="connected" stale={false} msgAgeMs={3000} collapsed={true} />
    );
    const dot = container.querySelector('.status-box-collapsed');
    expect(dot).not.toBeNull();
    expect(dot.className).toContain('status-dot-ok');
    expect(dot.title).toBe('Connected · updated 3s ago');
    expect(container.querySelector('.status-box')).toBeNull();
  });
});
