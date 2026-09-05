import { createCard } from './card.js';
import { applyTooltip } from '../tooltip.js';

export function statusFields(status) {
  const fields = [
    ['GSMTAP listen', status.gsmtapListen, '', 'UDP address where GSMTAP packets are received.'],
    ['Received', status.stats.received, '', 'Total UDP datagrams received by the workbench.'],
    ['Decode failures', status.stats.parseFailed, status.stats.parseFailed ? 'warning' : '', 'Datagrams that could not be decoded as valid GSMTAP packets.'],
    ['Receive queue drops', status.stats.ingressDropped, status.stats.ingressDropped ? 'warning' : '', 'Datagrams discarded because the packet-processing queue was full.'],
    ['History evicted', status.stats.historyDropped, status.stats.historyDropped ? 'warning' : '', 'Oldest records removed because the in-memory history reached capacity.'],
    ['Live updates missed', status.stats.uiEventsDropped, status.stats.uiEventsDropped ? 'warning' : '', 'Live browser update events missed because an SSE client fell behind.'],
  ];
  if (status.mode !== 'listen') fields.splice(1, 0, ['Forward target', status.gsmtapForward || 'Disabled', '', 'UDP destination used by relay forwarding or explicit modify-mode sends.']);
  if (status.mode === 'relay') fields.splice(3, 0,
    ['Forwarded', status.stats.forwardSent, '', 'Datagrams the operating system accepted for delivery to the configured UDP target.'],
    ['Forward failures', status.stats.forwardFailed, status.stats.forwardFailed ? 'warning' : '', 'Datagrams the workbench could not send to the configured UDP target.']);
  return fields;
}

export function renderStatusCard(node, status) {
  const card = createCard({ title: 'Workbench status', className: 'status-card' });
  const topLine = document.createElement('div');
  topLine.className = 'status-top-line';
  const modeBadge = document.createElement('strong');
  modeBadge.className = `mode-badge mode-${status.mode}`;
  modeBadge.textContent = status.mode.toUpperCase();
  applyTooltip(modeBadge, 'Mode', 'The active workbench operating mode: listen, relay, or modify.');
  topLine.append(modeBadge);
  const listen = document.createElement('strong');
  listen.className = 'status-endpoint';
  listen.textContent = status.gsmtapListen;
  applyTooltip(listen, 'GSMTAP listen', 'UDP address where GSMTAP packets are received.');
  topLine.append(listen);
  if (status.mode !== 'listen') {
    const forward = document.createElement('span');
    forward.className = 'status-forward';
    forward.textContent = `→ ${status.gsmtapForward || 'Disabled'}`;
    applyTooltip(forward, 'Forward target', 'UDP destination used for relay forwarding or explicit modify-mode sends.');
    topLine.append(forward);
  }
  card.append(topLine);

  const primary = document.createElement('div');
  primary.className = 'status-metrics status-primary';
  addStatusMetric(primary, status.stats.received, 'received', 'Total UDP datagrams received by the workbench.');
  if (status.mode === 'relay') addStatusMetric(primary, status.stats.forwardSent, 'forwarded', 'Datagrams the operating system accepted for delivery to the configured UDP target.');
  addStatusMetric(primary, status.stats.parseFailed, 'decode failure', 'Datagrams that could not be decoded as valid GSMTAP packets.', status.stats.parseFailed ? 'warning' : '');

  const diagnostics = document.createElement('div');
  diagnostics.className = 'status-metrics status-diagnostics';
  if (status.mode === 'relay') addStatusMetric(diagnostics, status.stats.forwardFailed, 'forward failures', 'Datagrams the workbench could not send to the configured UDP target.', status.stats.forwardFailed ? 'warning' : '');
  addStatusMetric(diagnostics, status.stats.ingressDropped, 'queue drops', 'Datagrams discarded because the packet-processing queue was full.', status.stats.ingressDropped ? 'warning' : '');
  addStatusMetric(diagnostics, status.stats.historyDropped, 'history evictions', 'Oldest records removed because the in-memory history reached capacity.', status.stats.historyDropped ? 'warning' : '');
  addStatusMetric(diagnostics, status.stats.uiEventsDropped, 'live-update misses', 'Live browser update events missed because an SSE client fell behind.', status.stats.uiEventsDropped ? 'warning' : '');
  card.append(primary, diagnostics);
  node.replaceChildren(card);
}

function addStatusMetric(parent, value, label, tooltip, className = '') {
  const metric = document.createElement('span');
  metric.className = `status-metric ${className}`.trim();
  applyTooltip(metric, `${value} ${label}`, tooltip);
  const number = document.createElement('strong');
  number.textContent = value;
  metric.append(number, ` ${label}`);
  parent.append(metric);
}
