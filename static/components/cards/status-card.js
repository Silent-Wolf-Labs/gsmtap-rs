import { createCard } from './card.js';
import { applyTooltip } from '../tooltip.js';

const captureIcons = {
  pause: '/styles/pause-icon-32x32.png',
  play: '/styles/play-icon-32x32.png',
};

export function statusFields(status) {
  return [
    ['Workbench Mode', displayMode(status.mode), '', 'The active workbench operating mode: listen, relay, or modify.'],
    ['Listen on', status.gsmtapListen, '', 'UDP address where GSMTAP packets are received.'],
    ['Forward to', status.gsmtapForward || 'Disabled', '', 'UDP destination used for relay forwarding or explicit modify-mode sends.'],
  ];
}

export function renderStatusCard(node, status, { onCaptureToggle } = {}) {
  const card = createCard({ title: 'Workbench status', className: 'status-card' });
  const layout = document.createElement('div');
  layout.className = 'status-layout';

  const information = document.createElement('div');
  information.className = 'status-information';
  information.append(
    createStatusRow('Workbench Mode', createModeControl(status)),
    createStatusRow('Listen on', status.gsmtapListen),
    createStatusRow('Forward to', status.gsmtapForward || 'Disabled'),
  );

  const captureControls = document.createElement('div');
  captureControls.className = 'status-capture-controls';
  const captureButton = document.createElement('button');
  captureButton.type = 'button';
  captureButton.className = `capture-toggle${status.capturePaused ? ' capture-paused' : ''}`;
  captureButton.setAttribute('aria-pressed', String(Boolean(status.capturePaused)));

  const captureIcon = document.createElement('img');
  captureIcon.className = 'capture-icon';
  captureIcon.src = status.capturePaused ? captureIcons.play : captureIcons.pause;
  captureIcon.alt = '';
  captureIcon.setAttribute('aria-hidden', 'true');
  const captureLabel = document.createElement('span');
  captureLabel.textContent = status.capturePaused ? 'Resume Capture' : 'Pause Capture';
  captureButton.append(captureIcon, captureLabel);

  const captureError = document.createElement('p');
  captureError.className = 'error capture-error';
  captureError.setAttribute('role', 'status');
  captureButton.addEventListener('click', async () => {
    captureButton.disabled = true;
    captureError.textContent = '';
    try {
      await onCaptureToggle?.(!status.capturePaused);
    } catch (error) {
      captureButton.disabled = false;
      captureError.textContent = `Unable to update capture state: ${error.message}`;
    }
  });
  captureControls.append(captureButton, captureError);

  layout.append(information, captureControls);
  card.append(layout);
  node.replaceChildren(card);
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
  const content = typeof value === 'string' ? document.createElement('strong') : value;
  if (typeof value === 'string') content.textContent = value;
  content.classList.add('status-row-value');
  row.append(name, content);
  return row;
}

function displayMode(mode) {
  return mode.charAt(0).toUpperCase() + mode.slice(1);
}
