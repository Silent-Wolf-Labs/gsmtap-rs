import { createBufferTable } from './buffer-table.js';
import { createCopyOutput } from './copy-output.js';

export function createBcdResult({ documentRef = document } = {}) {
  const node = documentRef.createElement('div'); node.className = 'conversion-result';
  const summary = documentRef.createElement('p'); summary.setAttribute('role', 'status'); summary.setAttribute('aria-live', 'polite');
  const hex = documentRef.createElement('pre'); hex.setAttribute('aria-label', 'BCD output (hex)');
  const text = documentRef.createElement('pre'); text.setAttribute('aria-label', 'BCD decoded text');
  const buffer = createBufferTable({ documentRef });
  const copyHex = createCopyOutput({ documentRef, label: 'Copy output hex' });
  const copyText = createCopyOutput({ documentRef, label: 'Copy decoded text' });
  const copyBuffer = createCopyOutput({ documentRef, label: 'Copy final buffer' });
  let last;
  return { node, update(output) {
    if (last === output.result) return;
    last = output.result;
    const result = output.result;
    for (const [control, value] of [[copyHex, result?.outputHex], [copyText, result?.outputText], [copyBuffer, result?.finalDestinationHex]])
      control.update(value ?? null);
    if (!result) {
      summary.textContent = 'No conversion result.'; summary.className = '';
      node.replaceChildren(summary); return;
    }
    const scalar = ['char2bcd', 'bcd2char'].includes(result.operation);
    summary.className = result.success ? 'success' : 'error';
    if (!result.success) summary.textContent = result.error?.message || 'Conversion failed. Inspect the final buffer.';
    else if (result.operation === 'bcd2str') {
      const displayed = result.outputText?.length ?? 0;
      summary.textContent = `Requested digits: ${result.returnValue}; displayed: ${displayed}.${displayed < result.returnValue ? ' Output truncated by destination capacity.' : ''}`;
    } else if (result.operation === 'str2bcd') summary.textContent = `Returned used-byte position: ${result.returnValue}. The table includes untouched bytes.`;
    else summary.textContent = `${result.operation === 'char2bcd' ? 'Nibble value' : 'Character byte value'}: ${result.returnValue}.`;
    hex.hidden = result.outputHex == null;
    hex.textContent = `${result.operation === 'str2bcd' ? 'Complete BCD destination' : 'Output bytes'}: ${result.outputHex ?? ''}`;
    text.hidden = result.outputText == null;
    text.textContent = `${result.success ? 'Decoded text' : 'Decoded text (partial or disallowed digits)'}: ${result.outputText ?? ''}`;
    copyHex.node.hidden = result.outputHex == null;
    copyText.node.hidden = result.outputText == null;
    copyBuffer.node.hidden = scalar;
    buffer.node.hidden = scalar;
    if (!scalar) buffer.update(result.initialDestinationHex, result.finalDestinationHex);
    node.replaceChildren(summary, hex, text, copyHex.node, copyText.node, copyBuffer.node, buffer.node);
  } };
}
