import React from 'react';
import { render } from '@testing-library/react';
import { describe, test, expect } from 'vitest';

import Statistics from '../Statistics.js';
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

const fixture = fixture_read('api/status.json');

function cellTexts(container) {
  return Array.from(container.querySelectorAll('.ant-table-cell'))
    .map((cell) => cell.textContent);
}

describe('Statistics', () => {
  test('renders every transport with dashes before the first status message', () => {
    const { container } = render(<Statistics txState={null} />);
    expect(container.querySelector('.ant-table')).not.toBeNull();
    expect(container.textContent).toContain('Transmit rates');
    for (const label of ['Bluetooth 4 legacy', 'Bluetooth 5 Long Range', 'Wi-Fi Beacon', 'Wi-Fi NAN']) {
      expect(container.textContent).toContain(label);
    }
    // Unknown values dash out instead of showing a misleading zero.
    const texts = cellTexts(container);
    expect(texts.filter((t) => t === '—')).toHaveLength(8);
  });

  test('renders per-transport transmit rates from the status message', () => {
    const { container } = render(<Statistics txState={fixture.tx} />);
    expect(container.querySelector('.ant-table')).not.toBeNull();
    for (const label of ['Bluetooth 4 legacy', 'Bluetooth 5 Long Range', 'Wi-Fi Beacon', 'Wi-Fi NAN']) {
      expect(container.textContent).toContain(label);
    }
    // Fixture values render verbatim: the bench steady state (all ten BT4
    // slots, N=3 packs, five IE refreshes, two NAN action frames).
    const texts = cellTexts(container);
    expect(texts).toContain('10');
    expect(texts).toContain('15');
    expect(texts).toContain('6');
  });

  test('dashes out transports the payload does not carry', () => {
    const { container } = render(<Statistics txState={{ bt4: { frames: 1, messages: 1 } }} />);
    expect(container.textContent).toContain('Bluetooth 4 legacy');
    const texts = cellTexts(container);
    expect(texts).toContain('1');
    expect(texts.filter((t) => t === '—')).toHaveLength(6);
  });
});
