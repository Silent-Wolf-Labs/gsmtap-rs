const names = ['version','headerLengthWords','messageType','timeslot','arfcn','signalDbm','snrDb','frameNumber','subtype','antennaNumber','subSlot','reserved'];
const fields = document.querySelector('#fields');
names.forEach((name, i) => { const label=document.createElement('label'); label.textContent=name; const input=document.createElement('input'); input.name=name; input.type='number'; input.value=i === 0 ? 2 : (i === 1 ? 4 : 0); label.append(input); fields.append(label); });
const render = packets => { document.querySelector('#packets').innerHTML = packets.slice().reverse().map(p => `<article><strong>${p.direction}</strong> ${new Date(Number(p.timestampMs)).toLocaleString()} — ${p.peer}<br><code>${p.rawHex}</code>${p.parseError ? `<p class="error">${p.parseError}</p>` : `<pre>${JSON.stringify(p.decoded, null, 2)}</pre>`}</article>`).join(''); };
fetch('/api/status').then(r=>r.json()).then(s => document.querySelector('#status').textContent=`RX ${s.gsmtapListen} · TX ${s.gsmtapTarget}`);
fetch('/api/packets').then(r=>r.json()).then(render);
const events = new EventSource('/api/events'); events.onmessage = e => fetch('/api/packets').then(r=>r.json()).then(render);
document.querySelector('#send-form').onsubmit = async e => { e.preventDefault(); const data=Object.fromEntries(new FormData(e.target)); names.forEach(n => data[n]=Number(data[n])); const response=await fetch('/api/encode-send',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify(data)}); document.querySelector('#result').textContent=await response.text(); };
