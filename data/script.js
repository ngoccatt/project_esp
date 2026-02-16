// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

const MAX_TEMP = 100
const MIN_TEMP = 0
const MAX_HUMID = 100
const MIN_HUMID = 0
const TEMP_HUMID_TIMEOUT_MS = 10000

var temp_humid_updated = false;
var lastTempHumidUpdate = 0;

window.addEventListener('load', onLoad);

function onLoad(event) {
    initWebSocket();
}

function onOpen(event) {
    console.log('Connection opened');
    
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function Send_Data(data) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(data);
        console.log("📤 Gửi:", data);
    } else {
        console.warn("⚠️ WebSocket chưa sẵn sàng: ", data);
        alert("⚠️ WebSocket chưa kết nối!");
    }
}

function onMessage(event) {
    console.log("📩 Nhận:", event.data);
    try {
        var data = JSON.parse(event.data);
        var pageType = data["page"];
        processMessage(pageType, data["value"]);
    } catch (e) {
        console.warn("Không phải JSON hợp lệ:", event.data);
    }
}

function processMessage(type, data) {
    if (type == "home")
    {
        let temp = parseFloat(data["temperature"]);
        let humidity = parseFloat(data["humidity"]);
        if (isNaN(temp) || temp === undefined || isNaN(humidity) || humidity === undefined) {
            console.warn("Dữ liệu thiếu trường 'temperature' hoặc 'humidity':", temp, humidity);
            return;
        }
        updateGauge('gauge_temp', temp, MIN_TEMP, MAX_TEMP);
        updateGauge('gauge_humi', humidity, MIN_HUMID, MAX_HUMID);
        temp_humid_updated = true;
        lastTempHumidUpdate = Date.now();
    }
    else if (type == "device")
    {
        relayList = [];
        if (!data || typeof data !== "object") {
            data = {};
        }
        Object.entries(data).forEach(([name, config]) => {
            relayList.push({ id: name+config.gpio, name, gpio: config.gpio, state: config.status });
        });
        renderRelays();
    }
    else if (type == "setting")
    {
        document.getElementById("ssid").value = data["ssid"] || "";
        document.getElementById("password").value = data["password"] || "";
        document.getElementById("token").value = data["token"] || "";
        document.getElementById("server").value = data["server"] || "";
        document.getElementById("port").value = data["port"] || "";
    }
}


// ==================== UI NAVIGATION ====================
let relayList = [];
let deleteTarget = null;

function showSection(id, event) {
    document.querySelectorAll('.section').forEach(sec => sec.style.display = 'none');
    document.getElementById(id).style.display = id === 'settings' ? 'flex' : 'block';
    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    event.currentTarget.classList.add('active');
    if (id === 'device') {
        const relayJSON = JSON.stringify({
                page: "device",
                type: "query",
                value: {}
            });
        Send_Data(relayJSON);
    }
    else if (id === 'settings') {
        const settingsJSON = JSON.stringify({
        page: "setting",
        type: "query",
        value: {}
        });
        Send_Data(settingsJSON);
    }
}


// ==================== HOME GAUGES ====================
function updateGauge(id, value, min, max) {
    const arc = document.getElementById(id + '_arc');
    const valueElement = document.getElementById(id + '_value');
    
    if (!arc || !valueElement) return;
    
    // Calculate percentage
    const percentage = Math.max(0, Math.min(100, ((value - min) / (max - min)) * 100));
    
    // Arc length is approximately 251.2 (half circle)
    const arcLength = 251.2;
    const offset = arcLength - (arcLength * percentage / 100);
    
    // Update arc
    arc.style.strokeDashoffset = offset;
    
    // Update value and limit the number to 2 decimal places
    valueElement.textContent = value.toFixed(2);
    
    // Update color based on value
    let color;
    if (id === 'gauge_temp') {
        // Temperature colors: cold to hot
        if (value < 10) {
            color = '#00BCD4'; // Cyan - cold
        } else if (value < 20) {
            color = '#4CAF50'; // Green - cool
        } else if (value < 30) {
            color = '#8BC34A'; // Light green - comfortable
        } else if (value < 35) {
            color = '#FFC107'; // Yellow - warm
        } else {
            color = '#F44336'; // Red - hot
        }
    } else if (id === 'gauge_humi') {
        // Humidity colors: low to high
        if (value < 30) {
            color = '#F44336'; // Red - too dry
        } else if (value < 50) {
            color = '#42A5F5'; // Light blue
        } else if (value < 70) {
            color = '#00BCD4'; // Cyan - comfortable
        } else {
            color = '#0288D1'; // Dark blue - humidity
        }
    }
    
    arc.style.stroke = color;
}

function checkTempHumidTimeout() {
    if (lastTempHumidUpdate === 0) return;
    if (Date.now() - lastTempHumidUpdate > TEMP_HUMID_TIMEOUT_MS) {
        temp_humid_updated = false;
    }
}

window.onload = function () {
    // Initialize gauges with default values
    let temp = Math.floor(Math.random() * (MAX_TEMP - MIN_TEMP + 1)) + MIN_TEMP;
    let humidity = Math.floor(Math.random() * (MAX_HUMID - MIN_HUMID + 1)) + MIN_HUMID;
    
    // Monitor temp/humidity updates
    setInterval(() => {
        checkTempHumidTimeout();
        if (!temp_humid_updated) {
            temp = Math.floor(Math.random() * (MAX_TEMP - MIN_TEMP + 1)) + MIN_TEMP;
            humidity = Math.floor(Math.random() * (MAX_HUMID - MIN_HUMID + 1)) + MIN_HUMID;
            updateGauge('gauge_temp', temp, MIN_TEMP, MAX_TEMP);
            updateGauge('gauge_humi', humidity, MIN_HUMID, MAX_HUMID);
        }
    }, 5000);
};


// ==================== DEVICE FUNCTIONS ====================
function openAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'flex';
}
function closeAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'none';
}
function saveRelay() {
    const name = document.getElementById('relayName').value.trim();
    const gpio = document.getElementById('relayGPIO').value.trim();
    if (!name || !gpio) return alert("⚠️ Please fill all fields!");
    relayList.push({ id: name+gpio, name, gpio, state: false });
    renderRelays();
    const relayJSON = JSON.stringify({
        page: "device",
        type: "request",
        value: {
            name: name,
            status: false,
            gpio: gpio,
            action: "create"
        }
    });
    Send_Data(relayJSON);
    closeAddRelayDialog();
}
function renderRelays() {
    const container = document.getElementById('relayContainer');
    container.innerHTML = "";
    relayList.forEach(r => {
        const card = document.createElement('div');
        card.className = 'device-card';
        card.innerHTML = `
      <i class="device-icon">⚡</i>
      <h3>${r.name}</h3>
      <p>GPIO: ${r.gpio}</p>
      <button class="toggle-btn ${r.state ? 'on' : ''}" onclick="toggleRelay('${r.id}')">
        ${r.state ? 'ON' : 'OFF'}
      </button>
      <i class="delete-icon" onclick="showDeleteDialog('${r.id}')">⛓️‍💥</i>
    `;
        container.appendChild(card);
    });
}
function toggleRelay(id) {
    console.log("Toggle function: " + id + relayList);
    const relay = relayList.find(r => String(r.id) == String(id));
    if (relay) {
        relay.state = !relay.state;
        const relayJSON = JSON.stringify({
            page: "device",
            type: "request",
            value: {
                name: relay.name,
                status: relay.state,
                gpio: relay.gpio,
                action: "control"
            }
        });
        Send_Data(relayJSON);
        renderRelays();
    }
}
function showDeleteDialog(id) {
    deleteTarget = id;
    document.getElementById('confirmDeleteDialog').style.display = 'flex';
}
function closeConfirmDelete() {
    document.getElementById('confirmDeleteDialog').style.display = 'none';
}
function confirmDelete() {
    const relay = relayList.find(r => String(r.id) == String(deleteTarget));
    const relayJSON = JSON.stringify({
        page: "device",
        type: "request",
        value: {
            name: relay.name,
            status: false,
            gpio: relay.gpio,
            action: "remove"
        }
    });
    Send_Data(relayJSON);
    relayList = relayList.filter(r => String(r.id) !== String(deleteTarget));
    renderRelays();
    closeConfirmDelete();
}


// ==================== SETTINGS FORM (BỔ SUNG) ====================
document.getElementById("settingsForm").addEventListener("submit", function (e) {
    e.preventDefault();

    const ssid = document.getElementById("ssid").value.trim();
    const password = document.getElementById("password").value.trim();
    const token = document.getElementById("token").value.trim();
    const server = document.getElementById("server").value.trim();
    const port = document.getElementById("port").value.trim();

    const settingsJSON = JSON.stringify({
        page: "setting",
        type: "request",
        value: {
            ssid: ssid,
            password: password,
            token: token,
            server: server,
            port: port
        }
    });

    Send_Data(settingsJSON);
    alert("✅ Cấu hình đã được gửi đến thiết bị!");
});
