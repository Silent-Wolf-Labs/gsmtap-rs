const TOOLS = new Set(['hexparse', 'bcd', 'bits', 'base64']);

export function createConversionService({ fetchImpl = globalThis.fetch, available = true, enabledTools = ['hexparse', 'bcd', 'bits', 'base64'] } = {}) {
  const isAvailable = tool => available && enabledTools.includes(tool);
  return {
    available,
    isAvailable,
    async convert(tool, payload, { signal } = {}) {
      if (!TOOLS.has(tool)) throw new Error(`Unknown conversion tool: ${tool}`);
      if (!isAvailable(tool)) throw new Error('This conversion tool is not available yet.');
      const response = await fetchImpl(`/api/conversions/${tool}`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify(payload),
        signal,
      });
      if (!response.ok) throw new Error((await response.text()) || `Conversion request failed (${response.status}).`);
      return response.json();
    },
  };
}
