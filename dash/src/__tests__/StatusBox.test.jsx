import React from 'react';
import { render } from '@testing-library/react';
import { describe, test, expect } from 'vitest';

import StatusBox from '../StatusBox.js';

describe('StatusBox', () => {
  test('carries the state on both the box and the dot', () => {
    // Two independent carriers on purpose: the tint conveys the state at a
    // glance, the dot keeps it readable without relying on colour alone.
    for (const state of ['ok', 'degraded', 'down']) {
      const { container } = render(<StatusBox state={state} label="Telemetry" />);
      const box = container.querySelector('.status-box');
      expect(box.className).toContain('status-box-' + state);
      expect(box.querySelector('.status-dot-' + state)).not.toBeNull();
    }
  });

  test('renders the label and the optional detail line', () => {
    const { container } = render(
      <StatusBox state="ok" label="GNSS" detail="3D fix" />
    );
    expect(container.querySelector('.status-box-label').textContent).toBe('GNSS');
    expect(container.querySelector('.status-box-detail').textContent).toBe('3D fix');
  });

  test('omits the detail line when there is nothing to say', () => {
    const { container } = render(<StatusBox state="degraded" label="GNSS" />);
    expect(container.querySelector('.status-box-detail')).toBeNull();
  });

  test('collapsed renders the icon plus a state dot, text in the tooltip', () => {
    const { container } = render(
      <StatusBox
        state="down"
        label="Disconnected"
        detail="since 12s ago"
        icon={<span data-testid="icon" />}
        collapsed={true}
      />
    );
    const item = container.querySelector('.status-box-collapsed');
    expect(item).not.toBeNull();
    expect(item.title).toBe('Disconnected · since 12s ago');
    // Identity comes from the icon, state from the dot beside it — the
    // collapsed element itself carries neither, so a regression that drops
    // the icon and re-tints the container cannot pass.
    expect(item.querySelector('[data-testid="icon"]')).not.toBeNull();
    expect(item.querySelector('.status-dot-down')).not.toBeNull();
    expect(item.className).not.toContain('status-dot');
    // No box, and no label text — there is no room for either.
    expect(container.querySelector('.status-box')).toBeNull();
    expect(container.textContent).toBe('');
  });

  test('collapsed with no detail tooltips the label alone', () => {
    // This is the device flags' case: they carry a colour and a name only.
    const { container } = render(
      <StatusBox state="down" label="GNSS" icon={<span />} collapsed={true} />
    );
    expect(container.querySelector('.status-box-collapsed').title).toBe('GNSS');
  });
});
