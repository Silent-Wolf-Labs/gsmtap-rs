import { createCard } from './card.js';
import { applyTooltip } from '../tooltip.js';

const captureIcons = {
  pause: '/styles/pause-icon-32x32.png',
  play: '/styles/play-icon-32x32.png',
};

const cardViews = new WeakMap();

export function statusFields(status) {
  return [
    ['Workbench Mode', displayMode(status.mode), '', 'The active workbench operating mode: listen, relay, or modify.'],
    ['Listen on', status.gsmtapListen, '', 'UDP address where GSMTAP packets are received.'],
    ['Forward to', status.gsmtapForward || 'Disabled', '', 'UDP destination used for relay forwarding or explicit modify-mode sends.'],
  ];
}

export function renderStatusCard(node, status, { onCaptureToggle } = {}) {
  let view = cardViews.get(node.firstElementChild);
  if (!view) {
    view = createStatusCard();
    node.replaceChildren(view.card);
    cardViews.set(view.card, view);
  }
  view.update(status, onCaptureToggle);
}

function createStatusCard() {
  const card = createCard({ title: 'Workbench status', className: 'status-card' });
  const layout = document.createElement('div');
  layout.className = 'status-layout';

  const information = document.createElement('div');
  information.className = 'status-information';
  const modeRow = createStatusRow('Workbench Mode');
  const listenRow = createStatusRow('Listen on');
  const forwardRow = createStatusRow('Forward to');
  information.append(modeRow.row, listenRow.row, forwardRow.row);

  const captureControls = document.createElement('div');
  captureControls.className = 'status-capture-controls';
  const captureButton = document.createElement('button');
  captureButton.type = 'button';
  captureButton.className = 'capture-toggle';
  const captureIcon = document.createElement('img');
  captureIcon.className = 'capture-icon';
  captureIcon.alt = '';
  captureIcon.setAttribute('aria-hidden', 'true');
  const captureLabel = document.createElement('span');
  captureButton.append(captureIcon, captureLabel);

  const captureError = document.createElement('p');
  captureError.className = 'error capture-error';
  captureError.setAttribute('role', 'status');
  const state = { status: undefined, onCaptureToggle: undefined, requestInFlight: false };
  captureButton.addEventListener('click', async () => {
    state.requestInFlight = true;
    captureButton.disabled = true;
    captureError.textContent = '';
    try {
      await state.onCaptureToggle?.(!state.status.capturePaused);
      state.requestInFlight = false;
    } catch (error) {
      state.requestInFlight = false;
      captureButton.disabled = false;
      captureError.textContent = `Unable to update capture state: ${error.message}`;
    }
  });
  captureControls.append(captureButton, captureError);

  layout.append(information, captureControls);
  card.append(layout);

  return {
    card,
    update(status, onCaptureToggle) {
      const previousStatus = state.status;
      state.status = status;
      state.onCaptureToggle = onCaptureToggle;
      updateStatusRow(modeRow, createModeControl(status));
      updateStatusRow(listenRow, status.gsmtapListen);
      updateStatusRow(forwardRow, status.gsmtapForward || 'Disabled');
      captureButton.className = `capture-toggle${status.capturePaused ? ' capture-paused' : ''}`;
      captureButton.setAttribute('aria-pressed', String(Boolean(status.capturePaused)));
      captureIcon.src = status.capturePaused ? captureIcons.play : captureIcons.pause;
      captureLabel.textContent = status.capturePaused ? 'Resume Capture' : 'Pause Capture';
      if (!state.requestInFlight || previousStatus?.capturePaused !== status.capturePaused) {
        captureButton.disabled = false;
      }
    },
  };
}

function createModeControl(status) {
  const mode = document.createElement('span');
  mode.className = `mode-control mode-${status.mode}`;
  const modeLabel = document.createElement('span');
  modeLabel.textContent = displayMode(status.mode);
  const chevron = document.createElement('span');
  chevron.className = 'mode-chevron';
  chevron.setAttribute('aria-hidden', 'true');
  chevron.textContent = '▾';
  mode.append(modeLabel, chevron);
  applyTooltip(mode, 'Mode', 'The active workbench operating mode: listen, relay, or modify.');
  return mode;
}

function createStatusRow(label, value) {
  const row = document.createElement('div');
  row.className = 'status-row';
  const name = document.createElement('span');
  name.className = 'status-row-label';
  name.textContent = `${label}:`;
  const content = document.createElement('strong');
  content.className = 'status-row-value';
  row.append(name, content);
  return { row, content };
}

function updateStatusRow(statusRow, value) {
  statusRow.content.replaceChildren(value instanceof Node ? value : document.createTextNode(value));
}

function displayMode(mode) {
  return mode.charAt(0).toUpperCase() + mode.slice(1);
}
