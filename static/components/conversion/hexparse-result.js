import { createBufferTable } from './buffer-table.js';
import { createCopyOutput } from './copy-output.js';

export function createHexparseResult({ documentRef = document } = {}) {
  const node = documentRef.createElement('div'); node.className = 'conversion-result';
  const summary = documentRef.createElement('p'); summary.setAttribute('role', 'status'); summary.setAttribute('aria-live', 'polite');
  const hex = documentRef.createElement('pre'); hex.setAttribute('aria-label', 'Parsed bytes (hex)');
  const text = documentRef.createElement('pre'); text.setAttribute('aria-label', 'Parsed text (UTF-8)');
  const buffer = createBufferTable({ documentRef });
  const copyHex = createCopyOutput({ documentRef, label: 'Copy parsed hex' });
  const copyText = createCopyOutput({ documentRef, label: 'Copy parsed text' });
  const copyBuffer = createCopyOutput({ documentRef, label: 'Copy final buffer' });
  node.append(summary, hex, text, copyHex.node, copyText.node, copyBuffer.node, buffer.node);
  let last;
  return { node, update(output) {
    if (last === output.result) return;
    last = output.result;
    const result = output.result;
    summary.textContent = !result ? 'No conversion result.' : result.success
      ? `Parsed ${result.returnValue} byte(s).` : result.error?.message || 'Parsing failed. The final buffer includes partial writes.';
    summary.className = result ? result.success ? 'success' : 'error' : '';
    hex.hidden = !result?.success; hex.textContent = `Parsed hex: ${result?.outputHex ?? ''}`;
    text.hidden = result?.outputText == null; text.textContent = `Parsed text: ${result?.outputText ?? ''}`;
    copyHex.update(result?.success ? result.outputHex ?? '' : null);
    copyText.update(result?.outputText ?? null);
    copyBuffer.update(result?.finalDestinationHex ?? null);
    for (const control of [copyHex, copyText, copyBuffer]) control.node.hidden = !result;
    buffer.node.hidden = !result;
    if (result) buffer.update(result.initialDestinationHex, result.finalDestinationHex);
  } };
}
