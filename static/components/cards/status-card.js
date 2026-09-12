import { createCard } from './card.js';
import { applyTooltip } from '../tooltip.js';

const captureIcons = { pause: '/styles/pause-icon-32x32.png', play: '/styles/play-icon-32x32.png' };
const cardViews = new WeakMap();

export function statusFields(status) {
  const fields = [['Workbench Mode', displayMode(status.mode), '', 'The active workbench operating mode: listen, relay, or modify.'], ['Listen on', status.gsmtapListen, '', 'UDP address where GSMTAP packets are received.']];
  if (forwards(status.mode)) fields.push(['Forward to', status.gsmtapForward || '', '', 'UDP destination used for relay forwarding or explicit modify-mode sends.']);
  return fields;
}

export function renderStatusCard(node, status, callbacks = {}) {
  let view = cardViews.get(node.firstElementChild);
  if (!view) { view = createStatusCard(); node.replaceChildren(view.card); cardViews.set(view.card, view); }
  view.update(status, callbacks);
}

function createStatusCard() {
  const card = createCard({ title: 'Workbench status', className: 'status-card' });
  const layout = document.createElement('div'); layout.className = 'status-layout';
  const information = document.createElement('div'); information.className = 'status-information';
  const modeRow = row('Workbench Mode'); const listenRow = row('Listen on'); const forwardRow = row('Forward to');
  information.append(modeRow.row, listenRow.row, forwardRow.row);
  const state = { status: undefined, callbacks: {}, captureInFlight: false, modeInFlight: false, pendingMode: null };
  let forwardControl;
  const refreshForward = () => {
    const visible = forwards(state.status?.mode) || state.pendingMode !== null;
    forwardRow.row.hidden = !visible;
    if (!visible) { forwardRow.row.replaceChildren(); return; }
    if (!forwardRow.row.firstChild) forwardRow.row.append(forwardRow.name, forwardRow.content);
    forwardControl.update();
  };
  const modeControl = createModeControl(state, refreshForward);
  update(modeRow, modeControl.wrapper);
  forwardControl = createForwardControl(state, refreshForward);
  update(forwardRow, forwardControl.wrapper);

  const controls = document.createElement('div'); controls.className = 'status-capture-controls';
  const captureButton = document.createElement('button'); captureButton.type = 'button'; captureButton.className = 'capture-toggle';
  const captureIcon = document.createElement('img'); captureIcon.className = 'capture-icon'; captureIcon.alt = ''; captureIcon.setAttribute('aria-hidden', 'true');
  const captureLabel = document.createElement('span'); captureButton.append(captureIcon, captureLabel);
  const captureError = document.createElement('p'); captureError.className = 'error capture-error'; captureError.setAttribute('role', 'status');
  captureButton.addEventListener('click', async () => {
    state.captureInFlight = true; captureButton.disabled = true; captureError.textContent = '';
    try { await state.callbacks.onCaptureToggle?.(!state.status.capturePaused); }
    catch (error) { captureError.textContent = `Unable to update capture state: ${error.message}`; }
    finally { state.captureInFlight = false; captureButton.disabled = false; }
  });
  controls.append(captureButton, captureError); layout.append(information, controls); card.append(layout);
  return { card, update(status, callbacks) {
    const previous = state.status; const modeChanged = previous && previous.mode !== status.mode; state.status = status; state.callbacks = callbacks;
    if (status.mode !== 'listen') state.pendingMode = null;
    if (status.mode === 'listen' && previous?.mode !== 'listen') forwardControl.clear();
    modeControl.update(modeChanged); update(listenRow, status.gsmtapListen); refreshForward();
    captureButton.className = `capture-toggle${status.capturePaused ? ' capture-paused' : ''}`;
    captureButton.setAttribute('aria-pressed', String(Boolean(status.capturePaused))); captureIcon.src = status.capturePaused ? captureIcons.play : captureIcons.pause;
    captureLabel.textContent = status.capturePaused ? 'Resume Capture' : 'Pause Capture';
    if (!state.captureInFlight || previous?.capturePaused !== status.capturePaused) captureButton.disabled = false;
  }};
}

function createModeControl(state, refreshForward) {
  const wrapper = document.createElement('div'); wrapper.className = 'mode-control-wrapper';
  const control = document.createElement('button'); control.type = 'button'; control.className = 'mode-control'; control.setAttribute('aria-haspopup', 'listbox'); control.setAttribute('aria-expanded', 'false');
  const label = document.createElement('span'); const chevron = document.createElement('span'); chevron.className = 'mode-chevron'; chevron.setAttribute('aria-hidden', 'true'); chevron.textContent = '▾'; control.append(label, chevron);
  const menu = document.createElement('div'); menu.className = 'mode-menu'; menu.hidden = true; menu.setAttribute('role', 'listbox');
  const error = document.createElement('p'); error.className = 'error mode-error'; error.setAttribute('role', 'status');
  const close = () => { menu.hidden = true; control.setAttribute('aria-expanded', 'false'); };
  for (const option of ['listen', 'relay', 'modify']) {
    const button = document.createElement('button'); button.type = 'button'; button.className = 'mode-option'; button.dataset.mode = option; button.textContent = displayMode(option); button.setAttribute('role', 'option');
    button.addEventListener('click', async () => {
      if (state.modeInFlight || option === state.status?.mode) return close();
      close(); error.textContent = '';
      if (state.status?.mode === 'listen' && option !== 'listen') { state.pendingMode = option; refreshForward(); return; }
      state.modeInFlight = true; control.disabled = true;
      try { await state.callbacks.onModeChange?.(option); }
      catch (exception) { error.textContent = `Unable to change mode: ${exception.message}`; state.callbacks.onModeError?.(exception); }
      finally { state.modeInFlight = false; control.disabled = false; }
    }); menu.append(button);
  }
  control.addEventListener('click', () => { if (!state.modeInFlight) { menu.hidden = !menu.hidden; control.setAttribute('aria-expanded', String(!menu.hidden)); } });
  control.addEventListener('keydown', event => { if (event.key === 'Escape') close(); });
  menu.addEventListener('keydown', event => { if (event.key === 'Escape') { close(); control.focus(); } });
  document.addEventListener('click', event => { if (!wrapper.contains(event.target)) close(); });
  wrapper.append(control, menu, error); applyTooltip(control, 'Mode', 'The active workbench operating mode: listen, relay, or modify.');
  return { wrapper, update(modeChanged) { if (modeChanged) close(); control.className = `mode-control mode-${state.status.mode}`; control.disabled = state.modeInFlight; label.textContent = displayMode(state.status.mode); for (const option of menu.children) option.setAttribute('aria-selected', String(option.dataset.mode === state.status.mode)); }};
}

function createForwardControl(state, refreshForward) {
  const wrapper = document.createElement('div'); wrapper.className = 'forward-control-wrapper';
  const input = document.createElement('input'); input.type = 'text'; input.className = 'forward-address'; input.placeholder = 'host:port'; input.setAttribute('aria-label', 'Forward address');
  const save = document.createElement('button'); save.type = 'button'; save.className = 'forward-address-save';
  const cancel = document.createElement('button'); cancel.type = 'button'; cancel.className = 'forward-address-cancel'; cancel.textContent = 'Cancel';
  const error = document.createElement('p'); error.className = 'error forward-address-error'; error.setAttribute('role', 'status');
  const targetMode = () => state.pendingMode || state.status?.mode;
  save.addEventListener('click', async () => {
    const mode = targetMode(); const forwardAddress = input.value.trim();
    if (!forwardAddress) { error.textContent = 'Enter a forward address in host:port format.'; return; }
    error.textContent = ''; save.disabled = true;
    try { await state.callbacks.onModeChange?.(mode, forwardAddress); }
    catch (exception) { error.textContent = `Unable to change mode: ${exception.message}`; state.callbacks.onModeError?.(exception); }
    finally { save.disabled = false; }
  });
  cancel.addEventListener('click', () => { state.pendingMode = null; input.value = ''; error.textContent = ''; refreshForward(); });
  wrapper.append(input, save, cancel, error);
  return { wrapper, clear() { input.value = ''; }, update() { const pending = state.pendingMode !== null; if (!pending && forwards(state.status?.mode) && document.activeElement !== input) input.value = state.status.gsmtapForward || ''; save.textContent = pending ? `Activate ${displayMode(state.pendingMode)}` : 'Save address'; cancel.hidden = !pending; }};
}

function row(label) { const element = document.createElement('div'); element.className = 'status-row'; const name = document.createElement('span'); name.className = 'status-row-label'; name.textContent = `${label}:`; const content = document.createElement('strong'); content.className = 'status-row-value'; element.append(name, content); return { row: element, name, content }; }
function update(statusRow, value) { statusRow.content.replaceChildren(value instanceof Node ? value : document.createTextNode(value)); }
function displayMode(mode) { return mode.charAt(0).toUpperCase() + mode.slice(1); }
function forwards(mode) { return mode === 'relay' || mode === 'modify'; }
