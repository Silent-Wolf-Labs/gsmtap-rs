import { readFileSync } from 'node:fs';

const stylesRoot = new URL('../../static/styles/', import.meta.url);
const baseStyles = readFileSync(new URL('base.css', stylesRoot), 'utf8');
const formStyles = readFileSync(new URL('forms.css', stylesRoot), 'utf8');
const packetTableStyles = readFileSync(new URL('packet-table.css', stylesRoot), 'utf8');
const entryStyles = readFileSync(new URL('style.css', stylesRoot), 'utf8');

test('stylesheet entry point imports the shared style layers', () => {
  expect(entryStyles).toContain('@import "./base.css";');
  expect(entryStyles).toContain('@import "./forms.css";');
  expect(entryStyles).toContain('@import "./packet-table.css";');
});

test('clear-history dialog uses the app card and button visual contracts', () => {
  expect(packetTableStyles).toMatch(/\.clear-history-dialog\s*\{[^}]*position:fixed;[^}]*inset:50% auto auto 50%;[^}]*transform:translate\(-50%, -50%\);/);
  expect(packetTableStyles).toContain('.clear-history-dialog::backdrop');
  expect(baseStyles).toContain('.card {');
  expect(formStyles).toMatch(/button\s*\{[^}]*background:var\(--bg-control\);[^}]*border:1px solid var\(--border-control-strong\);/);
});
