# ESP32-S3 XT/AT/PS2 Keymap System (Extended 256-element)
Detta dokument beskriver hur keymap-ex-systemet fungerar, hur man exporterar, importerar, redigerar och versionerar keymaps.

## Filstruktur
All keymap-data lagras i:
- `/keymap_ex.json` (LittleFS)

Formatet är en array med 256 objekt:
```json
[
  { "usb":0, "base":0, "shift":0, "altgr":0, "ctrl":0, "dead":0 },
  { "usb":1, "base":0, "shift":0, "altgr":0, "ctrl":0, "dead":0 },
  ...
  { "usb":255, ... }
]
```

## API
### Hämta aktuell keymap
```
GET /api/map_ex
```

### Spara en enskild tangent
```
POST /api/map_ex_set
BODY:
{
  "usb": 30,
  "base": 0x1C,
  "shift": 0x1C,
  "altgr": 0,
  "ctrl": 0,
  "dead": 0
}
```

### Ladda upp en full keymap (import)
```
POST /api/map_ex_upload
BODY: [256 objekt]
```

### Ladda ner (exportera)
```
GET /api/map_ex_download
```

### Återställa till standard
```
POST /api/map_ex_reset
```

---

## Versionshantering
Enheten kan hosta:
```
/keymap_ex.json
/keymap_ex.version.json
```

Exempel:
```json
{
  "version": "2025-01-01",
  "checksum": "4F9A-CC12-77B0-9AA1"
}
```

Keymap Editor kan visa versionsinfo och jämföra mot lokalt uppladdad map.

---

## Rekommenderad arbetsprocess
1. Öppna `keymap_editor.html`  
2. Redigera tangenter  
3. Klicka *Ladda ner JSON* för att exportera säkerhetskopia  
4. Vid byte av firmware: ladda upp den sparade `.json` via *Ladda upp JSON*  
5. Kontrollera att versionen matchar  

---

## Kompatibilitet (XT/AT/PS/2)
- XT = 1-byte scancodes  
- AT/PS/2 = Set 1, 2 eller 3  
Systemet använder interna tabeller för att generera rätt make/break enligt vald protokolltyp.
