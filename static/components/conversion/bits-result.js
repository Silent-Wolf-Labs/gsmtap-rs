import { createBufferTable } from './buffer-table.js';
import { createCopyOutput } from './copy-output.js';

export function createBitsResult({ documentRef = document } = {}) {
  const node = documentRef.createElement('div'); node.className = 'conversion-result';
  const summary = documentRef.createElement('p'); summary.setAttribute('role', 'status'); summary.setAttribute('aria-live', 'polite');
  const hex = documentRef.createElement('pre'); hex.setAttribute('aria-label', 'Complete destination (hex)');
  const binary = documentRef.createElement('pre'); binary.setAttribute('aria-label', 'Complete destination (binary)');
  const unpacked = documentRef.createElement('pre'); unpacked.setAttribute('aria-label', 'Unpacked output bits');
  const buffer = createBufferTable({ documentRef });
  const copyBuffer = createCopyOutput({ documentRef, label: 'Copy final buffer' });
  const copyBits = createCopyOutput({ documentRef, label: 'Copy unpacked bits' });
  let last;
  return { node, update(output) {
    if (last === output.result) return;
    last = output.result;
    const result = output.result;
    copyBuffer.update(result?.finalDestinationHex ?? null);
    copyBits.update(result?.outputText ?? null);
    if (!result) {
      summary.textContent = 'No conversion result.'; summary.className = '';
      node.replaceChildren(summary); return;
    }
    const extended = result.operation.endsWith('-ext');
    summary.className = result.success ? 'success' : 'error';
    summary.textContent = !result.success ? result.error?.message || 'Conversion failed. Inspect the final buffer.'
      : extended ? `Returned ending byte position: ${result.returnValue}. This is not the number of bytes written.`
      : `Returned byte count: ${result.returnValue}.`;
    hex.textContent = `Complete destination: ${result.finalDestinationHex ?? ''}`;
    binary.hidden = result.operation.startsWith('unpack');
    binary.textContent = `Complete destination (binary): ${(result.finalDestinationHex?.match(/[0-9a-f]{2}/gi) || []).map(byte => parseInt(byte, 16).toString(2).padStart(8, '0')).join(' ')}`;
    unpacked.hidden = result.outputText == null;
    unpacked.textContent = `Unpacked output bits: ${result.outputText ?? ''}`;
    copyBits.node.hidden = result.outputText == null;
    buffer.update(result.initialDestinationHex, result.finalDestinationHex);
    node.replaceChildren(summary, hex, binary, unpacked, copyBuffer.node, copyBits.node, buffer.node);
  } };
}
