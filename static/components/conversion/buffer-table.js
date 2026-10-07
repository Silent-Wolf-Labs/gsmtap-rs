const PAGE_SIZE = 128;
const bytes = value => (value || '').match(/[0-9a-f]{2}/gi)?.map(byte => byte.toUpperCase()) || [];

export function createBufferTable({ documentRef = document } = {}) {
  const node = documentRef.createElement('div');
  node.className = 'conversion-buffer';
  const table = documentRef.createElement('table');
  const caption = table.createCaption(); caption.textContent = 'Complete destination buffer';
  const head = table.createTHead().insertRow();
  for (const title of ['Offset (bytes)', 'Initial byte (hex)', 'Final byte (hex)', 'Changed']) {
    const cell = documentRef.createElement('th'); cell.scope = 'col'; cell.textContent = title; head.append(cell);
  }
  const body = table.createTBody();
  const navigation = documentRef.createElement('div'); navigation.className = 'conversion-buffer-navigation';
  const previous = documentRef.createElement('button'); previous.type = 'button'; previous.textContent = 'Previous bytes';
  const next = documentRef.createElement('button'); next.type = 'button'; next.textContent = 'Next bytes';
  const range = documentRef.createElement('span'); range.setAttribute('role', 'status');
  navigation.append(previous, range, next); node.append(table, navigation);
  let initial = [], final = [], page = 0;
  function render() {
    body.replaceChildren();
    const start = page * PAGE_SIZE;
    const end = Math.min(start + PAGE_SIZE, final.length);
    for (let offset = start; offset < end; offset++) {
      const row = body.insertRow();
      const changed = initial[offset] !== undefined && initial[offset] !== final[offset];
      if (changed) row.className = 'conversion-byte-changed';
      for (const value of [offset, initial[offset] ?? '—', final[offset], initial[offset] === undefined ? '—' : changed ? 'Yes' : 'No'])
        row.insertCell().textContent = String(value);
    }
    range.textContent = final.length ? `Bytes ${start + 1}–${end} of ${final.length}` : 'Empty destination buffer';
    previous.disabled = page === 0;
    next.disabled = end >= final.length;
    navigation.hidden = final.length <= PAGE_SIZE;
  }
  previous.addEventListener('click', () => { if (page > 0) { page--; render(); } });
  next.addEventListener('click', () => { if ((page + 1) * PAGE_SIZE < final.length) { page++; render(); } });
  return { node, update(before, after) { initial = bytes(before); final = bytes(after); page = 0; render(); } };
}
