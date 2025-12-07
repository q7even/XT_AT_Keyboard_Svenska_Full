let keymapEx = [];
let selectedIndex = -1;

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('btnReload').addEventListener('click', loadMap);
  document.getElementById('btnDownload').addEventListener('click', downloadMap);
  document.getElementById('btnUpload').addEventListener('click', ()=>document.getElementById('fileUp').click());
  document.getElementById('fileUp').addEventListener('change', uploadFile);
  document.getElementById('btnReset').addEventListener('click', resetMap);
  document.getElementById('btnSave').addEventListener('click', saveEntry);
  document.getElementById('btnCancel').addEventListener('click', clearEditor);
  buildGrid();
  loadMap();
});

function buildGrid(){
  const grid = document.getElementById('mapGrid');
  grid.innerHTML = '';
  for(let i=0;i<256;i++){
    const el = document.createElement('div');
    el.className = 'cell';
    el.id = 'cell-'+i;
    el.innerText = i.toString(16).toUpperCase().padStart(2,'0');
    el.addEventListener('click', ()=> openEditor(i));
    grid.appendChild(el);
  }
}

async function loadMap(){
  try {
    const r = await fetch('/api/map_ex');
    if (!r.ok) throw new Error('HTTP '+r.status);
    const arr = await r.json();
    keymapEx = arr;
    refreshGrid();
    clearEditor();
    alert('Keymap laddad');
  } catch(e){
    console.error(e);
    alert('Kunde inte ladda keymap: '+e.message);
  }
}

function refreshGrid(){
  for(let i=0;i<256;i++){
    const el = document.getElementById('cell-'+i);
    const entry = keymapEx[i];
    if (!entry) continue;
    if (entry.base && entry.base !== 0) el.innerText = toHex(entry.base);
    else el.innerText = i.toString(16).toUpperCase().padStart(2,'0');
  }
}

function openEditor(i){
  selectedIndex = i;
  const entry = keymapEx[i] || {base:0,shift:0,altgr:0,ctrl:0,dead:0};
  document.getElementById('editUsb').innerText = i + ' (0x'+ i.toString(16).toUpperCase().padStart(2,'0') +')';
  document.getElementById('editBase').value  = entry.base ? toHex(entry.base) : '';
  document.getElementById('editShift').value = entry.shift ? toHex(entry.shift) : '';
  document.getElementById('editAltgr').value = entry.altgr ? toHex(entry.altgr) : '';
  document.getElementById('editCtrl').value = entry.ctrl ? toHex(entry.ctrl) : '';
  document.getElementById('editDead').checked = !!entry.dead;
  document.getElementById('pvBase')?.innerText = entry.base ? toHex(entry.base) : '—';
  document.getElementById('pvShift')?.innerText = entry.shift ? toHex(entry.shift) : '—';
  document.getElementById('pvAltgr')?.innerText = entry.altgr ? toHex(entry.altgr) : '—';
  document.getElementById('pvCtrl')?.innerText = entry.ctrl ? toHex(entry.ctrl) : '—';
}

function clearEditor(){
  selectedIndex = -1;
  document.getElementById('editUsb').innerText = '—';
  document.getElementById('editBase').value = '';
  document.getElementById('editShift').value = '';
  document.getElementById('editAltgr').value = '';
  document.getElementById('editCtrl').value = '';
  document.getElementById('editDead').checked = false;
}

function toHex(n){ return n.toString(16).toUpperCase().padStart(2,'0'); }
function parseHex(s){ if (!s) return 0; s = s.trim(); if (s.startsWith('0x')||s.startsWith('0X')) s = s.slice(2); const v = parseInt(s,16); if (isNaN(v) || v < 0 || v > 255) return null; return v; }

async function saveEntry(){
  if (selectedIndex < 0) { alert('Välj en tangent först'); return; }
  const base = parseHex(document.getElementById('editBase').value);
  const shift = parseHex(document.getElementById('editShift').value);
  const altgr = parseHex(document.getElementById('editAltgr').value);
  const ctrl  = parseHex(document.getElementById('editCtrl').value);
  const dead  = document.getElementById('editDead').checked ? 1 : 0;
  if (base === null || shift === null || altgr === null || ctrl === null) {
    alert('Fel i hexkod — använd 00–FF eller lämna fält tomt för 0'); return;
  }
  const payload = { usb: selectedIndex, base: base, shift: shift, altgr: altgr, ctrl: ctrl, dead: dead };
  try {
    const r = await fetch('/api/map_ex_set', {
      method:'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify(payload)
    });
    if (!r.ok) throw new Error('HTTP '+r.status);
    keymapEx[selectedIndex] = payload;
    refreshGrid();
    clearEditor();
    alert('Sparat');
  } catch(e){
    console.error(e); alert('Kunde inte spara: '+e.message);
  }
}

function downloadMap(){ window.location = '/api/map_ex_download'; }

function uploadFile(ev){
  const f = ev.target.files[0];
  if (!f) return;
  const reader = new FileReader();
  reader.onload = async () => {
    try {
      const json = JSON.parse(reader.result);
      if (!Array.isArray(json) || json.length !== 256) { alert('Filen måste vara en array med 256 objekt'); return; }
      const r = await fetch('/api/map_ex_upload', { method:'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify(json) });
      if (!r.ok) throw new Error('HTTP '+r.status);
      alert('Upload lyckades — uppdaterar lokalt');
      loadMap();
    } catch(e){ console.error(e); alert('Parsing error: '+e.message); }
  };
  reader.readAsText(f);
}

async function resetMap(){ if (!confirm('Vill du återställa keymap till standard?')) return; const r = await fetch('/api/map_ex_reset', {method:'POST'}); if (r.ok) { alert('Reset gjord'); loadMap(); } else alert('Reset failed: '+r.status); }
