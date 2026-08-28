import React from 'react';
import { render } from '@testing-library/react';
import { describe, test, expect } from 'vitest';

import Status from '../Status.js';
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

// Presentational: the view carries the telemetry/GNSS flags only — the
// /ws connection lives in the sidebar ConnectionBox.
function rows(container) {
  const rows = container.querySelectorAll('.ant-row');
  expect(rows).toHaveLength(2);
  return rows;
}

describe('Status', () => {
  test('renders unknown states before the first status message', () => {
    const { container } = render(<Status telemetryState={null} gnssState={null} />);
    const [telemetryRow, gnssRow] = rows(container);
    expect(telemetryRow.querySelector('.anticon-question-circle')).not.toBeNull();
    expect(gnssRow.querySelector('.anticon-question-circle')).not.toBeNull();
  });

  test('maps each flag to its own row (telemetry ok, gnss failed)', () => {
    const { container } = render(
      <Status telemetryState={fixture.telemetry} gnssState={fixture.gnss} />
    );
    const [telemetryRow, gnssRow] = rows(container);
    expect(telemetryRow.textContent).toContain('Telemetry');
    expect(gnssRow.textContent).toContain('GNSS');
    expect(telemetryRow.querySelector('.anticon-check-circle')).not.toBeNull();
    expect(telemetryRow.querySelector('.anticon-close-circle')).toBeNull();
    expect(gnssRow.querySelector('.anticon-close-circle')).not.toBeNull();
    expect(gnssRow.querySelector('.anticon-check-circle')).toBeNull();
  });

  test('has no connection row — that lives in the sidebar box', () => {
    const { container } = render(
      <Status telemetryState={fixture.telemetry} gnssState={fixture.gnss} />
    );
    expect(container.textContent).not.toContain('Connection');
  });
});
