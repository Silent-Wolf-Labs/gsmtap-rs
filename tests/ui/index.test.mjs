import { readFileSync } from 'node:fs';

const indexHtml = readFileSync(new URL('../../static/index.html', import.meta.url), 'utf8');

beforeEach(() => {
  document.documentElement.innerHTML = indexHtml;
});

test('provides the stable, labeled workbench shell', () => {
  const requiredIds = [
    'status', 'send-section', 'selected-packet', 'send-form', 'fields',
    'preview-card', 'preview', 'confirm-send', 'result', 'filters', 'packets',
  ];

  for (const id of requiredIds) {
    expect(document.getElementById(id)).not.toBeNull();
  }

  expect(document.querySelector('main > header')).not.toBeNull();
  expect(document.querySelector('header h1').textContent).toBe('GSMTAP Workbench');
  expect(document.getElementById('status').getAttribute('aria-live')).toBe('polite');
  expect(document.getElementById('status').getAttribute('role')).toBe('status');
  expect(document.getElementById('result').getAttribute('aria-live')).toBe('polite');
  expect(document.getElementById('result').getAttribute('role')).toBe('status');

  const namedSections = [
    ['send-section', 'modify-selected-packet-heading'],
    ['preview-card', 'review-changes-heading'],
    ['packet-history', 'packet-history-heading'],
  ];
  for (const [sectionId, headingId] of namedSections) {
    const section = document.getElementById(sectionId);
    const heading = document.getElementById(headingId);
    expect(section.getAttribute('aria-labelledby')).toBe(headingId);
    expect(heading).not.toBeNull();
  }
  expect(new Set(namedSections.map(([, headingId]) => headingId)).size).toBe(namedSections.length);

  expect(document.getElementById('send-section').getAttribute('tabindex')).toBe('-1');
  expect(document.getElementById('send-section').hasAttribute('hidden')).toBe(true);
  expect(document.getElementById('preview-card').hasAttribute('hidden')).toBe(true);
  expect(document.getElementById('send-form').querySelector('#fields')).not.toBeNull();
  expect(document.getElementById('confirm-send').getAttribute('type')).toBe('button');
  expect(document.getElementById('confirm-send').disabled).toBe(true);
});