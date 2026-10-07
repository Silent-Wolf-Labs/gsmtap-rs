import { createBufferTable } from './buffer-table.js';
import { createCopyOutput } from './copy-output.js';

export function createBase64Result({ documentRef = document } = {}) {
  const node = documentRef.createElement('div'); node.className = 'conversion-result';
  const summary = documentRef.createElement('p'); summary.setAttribute('role', 'status'); summary.setAttribute('aria-live', 'polite');
  const text = documentRef.createElement('pre'); text.setAttribute('aria-label', 'Output text');
  const hex = documentRef.createElement('pre'); hex.setAttribute('aria-label', 'Output bytes');
  const buffer = createBufferTable({ documentRef });
  const copyText = createCopyOutput({ documentRef, label: 'Copy output text' });
  const copyHex = createCopyOutput({ documentRef, label: 'Copy output hex' });
  const copyBuffer = createCopyOutput({ documentRef, label: 'Copy final buffer' });
  let last;
  return { node, update(output) {
    if (last === output.result) return;
    last = output.result;
    const result = output.result;
    copyText.update(result?.outputText ?? null);
    copyHex.update(result?.outputHex ?? null);
    copyBuffer.update(result && !result.sizeProbe ? result.finalDestinationHex : null);
    if (!result) { summary.textContent = 'No conversion result.'; node.replaceChildren(summary); return; }
    const required = result.error?.code === 'bufferTooSmall';
    summary.className = result.success || (result.sizeProbe && required) ? 'success' : 'error';
    summary.textContent = `Return code: ${result.returnValue}. Output-length value: ${result.outputLength}. `
      + (required ? `Required destination capacity: ${result.outputLength} bytes.`
        : !result.success ? result.error?.message || 'Conversion failed.'
        : result.sizeProbe ? 'No destination was allocated; empty input preserves the initial length.'
        : result.operation === 'encode' ? 'Encoded text excludes the trailing NUL; the buffer includes it.'
        : 'Empty or whitespace-only input preserves the initial length; output below shows only converted bytes.');
    text.hidden = result.outputText == null;
    text.textContent = `${result.operation === 'encode' ? 'Base64 text' : 'Decoded text'}: ${result.outputText ?? ''}`;
    hex.hidden = result.outputHex == null; hex.textContent = `Output bytes: ${result.outputHex ?? ''}`;
    copyText.node.hidden = result.outputText == null;
    copyHex.node.hidden = result.outputHex == null;
    copyBuffer.node.hidden = Boolean(result.sizeProbe);
    buffer.node.hidden = Boolean(result.sizeProbe);
    buffer.update(result.initialDestinationHex, result.finalDestinationHex);
    node.replaceChildren(summary, text, hex, copyText.node, copyHex.node, copyBuffer.node, buffer.node);
  } };
}
