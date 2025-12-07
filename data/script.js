let mapCache = [];

window.onload = () => {
    loadFirmware();
    loadWiFi();
    loadMode();
    loadMap();
    buildKeyboard();
    buildUSBKeyList();
};

function loadFirmware() {
    fetch("/api/fw")
    .then(r => r.json())
    .then(j => {
        document.getElementById("fwver").innerText = "Firmware: " + j.version;
    });
}

function loadWiFi() {
    fetch("/api/wifi")
    .then(r => r.json())
    .then(j => {
        document.getElementById("sta_ssid").value = j.ssid;
    });
}

function saveWiFi() {
    let ssid = document.getElementById("sta_ssid").value;
    let pass = document.getElementById("sta_pass").value;
    fetch("/api/wifi_set", {
        method: "POST",
        headers: {"Content-Type":"application/json"},
        body: JSON.stringify({ssid:ssid, pass:pass})
    })
    .then(()=> {
        document.getElementById("wifi_status").innerText = "Sparat, försöker ansluta...";
    });
}

function loadMode() {
    fetch("/api/mode")
    .then(r=>r.json())
    .then(j=>{
        document.getElementById("mode").value = j.mode;
    });
}

function saveMode() {
    let mode = document.getElementById("mode").value;
    fetch("/api/mode_set", {
        method:"POST",
        headers:{"Content-Type":"application/json"},
        body:JSON.stringify({mode:mode})
    });
}

function buildKeyboard() {
    const keys = [
        "§","1","2","3","4","5","6","7","8","9","0","+","´",
        "Q","W","E","R","T","Y","U","I","O","P","Å","¨",
        "A","S","D","F","G","H","J","K","L","Ö","Ä","'",
        "<","Z","X","C","V","B","N","M",",",".","-"," "
    ];
    let kbd = document.getElementById("kbd");
    kbd.innerHTML = "";
    keys.forEach(k => {
        let d = document.createElement("div");
        d.className = "k";
        d.innerText = k;
        d.onclick = () => sendKey(k);
        kbd.appendChild(d);
    });
}

function sendKey(k) {
    fetch("/api/send_key", {
        method:"POST",
        headers:{"Content-Type":"application/json"},
        body:JSON.stringify({key:k})
    });
}

function buildUSBKeyList() {
    let sel = document.getElementById("usbKey");
    for (let i=0;i<256;i++) {
        let o = document.createElement("option");
        o.value = i;
        o.innerText = "USB " + i;
        sel.appendChild(o);
    }
}

function loadMap() {
    fetch("/api/map")
    .then(r=>r.json())
    .then(j=>{
        mapCache = j;
        dumpMap();
    });
}

function dumpMap() {
    let out = "";
    for (let i=0;i<mapCache.length;i++) {
        if (mapCache[i] !== 0)
            out += `${i} → ${mapCache[i].toString(16)}\n`;
    }
    document.getElementById("mapdump").innerText = out;
}

function applyMap() {
    let usb = document.getElementById("usbKey").value;
    let xt  = document.getElementById("xtVal").value;
    fetch("/api/map_set", {
        method:"POST",
        headers:{"Content-Type":"application/json"},
        body:JSON.stringify({usb:parseInt(usb), xt:parseInt(xt,16)})
    }).then(loadMap);
}
