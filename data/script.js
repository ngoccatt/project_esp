// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

const MAX_TEMP = 100
const MIN_TEMP = 0
const MAX_HUMID = 100
const MIN_HUMID = 0
const MAX_ANOMALY = 1
const MIN_ANOMALY = 0
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
        let anomaly = parseFloat(data["anomaly"]);
        if (isNaN(temp) || temp === undefined || isNaN(humidity) || humidity === undefined) {
            console.warn("Dữ liệu thiếu trường 'temperature' hoặc 'humidity':", temp, humidity);
            return;
        }
        updateGauge('gauge_temp', temp, MIN_TEMP, MAX_TEMP);
        updateGauge('gauge_humi', humidity, MIN_HUMID, MAX_HUMID);
        updateGauge('gauge_anomaly', anomaly, MIN_ANOMALY, MAX_ANOMALY);
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


// ==================== AI INFO UPLOAD ====================
let uploadProgressTimer = null;
const IMAGE_MAX_WIDTH = 96;
const IMAGE_MAX_HEIGHT = 96;
const IMAGE_JPEG_QUALITY = 0.65;
const IMAGE_CHUNK_SIZE = 1024;

function initAiUploadUI() {
    const aiImageInput = document.getElementById('aiImageInput');
    if (!aiImageInput) return;

    aiImageInput.addEventListener('change', handleAiImageSelected);
}

function triggerAiImageInput() {
    const aiImageInput = document.getElementById('aiImageInput');
    if (aiImageInput) {
        aiImageInput.click();
    }
}

function handleAiImageSelected(event) {
    const file = event.target.files && event.target.files[0];
    const uploadStatus = document.getElementById('uploadStatus');
    const previewWrapper = document.getElementById('uploadedPreviewWrapper');
    const imagePreview = document.getElementById('uploadedImagePreview');
    const aiResultText = document.getElementById('aiResultText');

    if (!file || !uploadStatus || !previewWrapper || !imagePreview || !aiResultText) return;

    if (!file.type.startsWith('image/')) {
        uploadStatus.textContent = 'Trạng thái: File không hợp lệ. Vui lòng chọn ảnh.';
        return;
    }

    console.log("Selected file:", file.name, file.type, file.size);
    console.log("File object:", file);

    const localUrl = URL.createObjectURL(file);
    imagePreview.src = localUrl;
    previewWrapper.style.display = 'block';
    aiResultText.textContent = 'Kết quả AI: Đang phân tích...';

    // Keep UI fake-upload flow while sending a real compressed/chunked payload in background.
    sendCompressedImageChunked(file).catch((err) => {
        console.error('Chunked transfer error:', err);
    });

    simulateUploadStatus(file.name);
}

async function sendCompressedImageChunked(file) {
    const compressedBlob = await compressImageFile(file, IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT, IMAGE_JPEG_QUALITY);
    const base64Payload = await blobToBase64String(compressedBlob);
    const imageId = `${Date.now()}_${Math.random().toString(16).slice(2, 8)}`;
    const totalChunks = Math.ceil(base64Payload.length / IMAGE_CHUNK_SIZE);

    const startMessage = JSON.stringify({
        page: 'ai_detect',
        type: 'image_start',
        value: {
            imageId: imageId,
            originalName: file.name,
            originalSize: file.size,
            mimeType: 'image/jpeg',
            compressedSize: compressedBlob.size,
            chunkSize: IMAGE_CHUNK_SIZE,
            totalChunks: totalChunks
        }
    });
    Send_Data(startMessage);

    for (let index = 0; index < totalChunks; index++) {
        const start = index * IMAGE_CHUNK_SIZE;
        const end = start + IMAGE_CHUNK_SIZE;
        const chunk = base64Payload.slice(start, end);

        const chunkMessage = JSON.stringify({
            page: 'ai_detect',
            type: 'image_chunk',
            value: {
                imageId: imageId,
                index: index,
                chunk: chunk
            }
        });
        Send_Data(chunkMessage);

        // Yield to event loop to avoid blocking UI with many chunks.
        await sleep(0);
    }

    const endMessage = JSON.stringify({
        page: 'ai_detect',
        type: 'image_end',
        value: {
            imageId: imageId,
            totalChunks: totalChunks
        }
    });
    Send_Data(endMessage);
}

function compressImageFile(file, maxWidth, maxHeight, quality) {
    return new Promise((resolve, reject) => {
        const image = new Image();
        const objectUrl = URL.createObjectURL(file);

        image.onload = () => {
            let targetWidth = image.width;
            let targetHeight = image.height;

            const ratio = Math.min(maxWidth / targetWidth, maxHeight / targetHeight, 1);
            targetWidth = Math.round(targetWidth * ratio);
            targetHeight = Math.round(targetHeight * ratio);

            const canvas = document.createElement('canvas');
            canvas.width = targetWidth;
            canvas.height = targetHeight;

            const context = canvas.getContext('2d');
            if (!context) {
                URL.revokeObjectURL(objectUrl);
                reject(new Error('Cannot get canvas context'));
                return;
            }

            context.drawImage(image, 0, 0, targetWidth, targetHeight);
            canvas.toBlob((blob) => {
                URL.revokeObjectURL(objectUrl);
                if (!blob) {
                    reject(new Error('Compression failed: empty blob'));
                    return;
                }
                resolve(blob);
            }, 'image/jpeg', quality);
        };

        image.onerror = () => {
            URL.revokeObjectURL(objectUrl);
            reject(new Error('Cannot decode selected image'));
        };

        image.src = objectUrl;
    });
}

function blobToBase64String(blob) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = () => {
            const result = String(reader.result || '');
            const commaIndex = result.indexOf(',');
            resolve(commaIndex >= 0 ? result.slice(commaIndex + 1) : result);
        };
        reader.onerror = () => reject(new Error('Cannot convert blob to base64'));
        reader.readAsDataURL(blob);
    });
}

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function simulateUploadStatus(fileName) {
    const uploadStatus = document.getElementById('uploadStatus');
    const aiResultText = document.getElementById('aiResultText');
    if (!uploadStatus || !aiResultText) return;

    if (uploadProgressTimer) {
        clearInterval(uploadProgressTimer);
        uploadProgressTimer = null;
    }

    let progress = 0;
    uploadStatus.textContent = `Trạng thái: Đang tải lên... ${progress}%`;

    uploadProgressTimer = setInterval(() => {
        const step = Math.floor(Math.random() * 20) + 8;
        progress = Math.min(100, progress + step);
        uploadStatus.textContent = `Trạng thái: Đang tải lên... ${progress}%`;

        if (progress >= 100) {
            clearInterval(uploadProgressTimer);
            uploadProgressTimer = null;
            uploadStatus.textContent = `Trạng thái: Tải lên thành công (${fileName}).`;
            aiResultText.textContent = `Kết quả AI: ${mockClassifyWaste(fileName)}`;
        }
    }, 180);
}

function mockClassifyWaste(fileName) {
    const lowerName = String(fileName || '').toLowerCase();

    if (lowerName.includes('plastic') || lowerName.includes('pet') || lowerName.includes('nhua')) {
        return 'Rác tái chế - Nhựa';
    }
    if (lowerName.includes('paper') || lowerName.includes('giay') || lowerName.includes('carton')) {
        return 'Rác tái chế - Giấy';
    }
    if (lowerName.includes('metal') || lowerName.includes('kimloai') || lowerName.includes('can')) {
        return 'Rác tái chế - Kim loại';
    }
    if (lowerName.includes('food') || lowerName.includes('organic') || lowerName.includes('huuco')) {
        return 'Rác hữu cơ';
    }

    return 'Rác hỗn hợp (mô phỏng)';
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
    else if (id === 'gauge_anomaly') {
        color = value > 0.5 ? '#FF5722' : '#4CAF50';
        const conclusion = document.getElementById('gauge_anomaly_conclusion');
        if (conclusion) {
            conclusion.textContent = value > 0.5 ? 'Bất thường' : 'Bình thường';
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
    initAiUploadUI();

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
            updateGauge('gauge_anomaly', Math.random(), 0, 1); // Simulate anomaly value
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
