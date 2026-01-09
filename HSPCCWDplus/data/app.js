// PCCWD Bridge Pin Configuration App

const state = {
    config: null,
    originalConfig: null,
    selectedPin: null,
    dirty: false,
    ws: null,
    wsConnected: false
};

// Type abbreviations for display
const typeAbbrev = {
    unused: '',
    motion: 'MOT',
    leak: 'LEAK',
    smoke: 'SMK',
    co2: 'CO2',
    valve: 'VLV'
};

// API functions
async function fetchConfig() {
    try {
        const response = await fetch('/api/config');
        if (!response.ok) throw new Error('Failed to fetch config');
        return await response.json();
    } catch (error) {
        console.error('Error fetching config:', error);
        showError('Failed to load configuration');
        return null;
    }
}

async function saveConfig(config) {
    try {
        const response = await fetch('/api/config', {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        if (!response.ok) throw new Error('Failed to save config');
        return await response.json();
    } catch (error) {
        console.error('Error saving config:', error);
        showError('Failed to save configuration');
        return null;
    }
}

async function fetchSystemInfo() {
    try {
        const response = await fetch('/api/system/info');
        if (!response.ok) throw new Error('Failed to fetch system info');
        return await response.json();
    } catch (error) {
        console.error('Error fetching system info:', error);
        return null;
    }
}

async function restartDevice() {
    try {
        await fetch('/api/system/restart', { method: 'POST' });
    } catch (error) {
        // Expected to fail as device restarts
    }
}

// UI rendering
function renderPinGrid() {
    const portA = document.getElementById('port-a');
    const portB = document.getElementById('port-b');

    portA.innerHTML = '';
    portB.innerHTML = '';

    if (!state.config || !state.config.pins) return;

    state.config.pins.forEach((pin, index) => {
        const pinEl = createPinElement(pin);
        if (index < 8) {
            portA.appendChild(pinEl);
        } else {
            portB.appendChild(pinEl);
        }
    });
}

function createPinElement(pin) {
    const el = document.createElement('div');
    el.className = `pin type-${pin.type}`;
    el.dataset.pin = pin.pin;

    const numEl = document.createElement('div');
    numEl.className = 'pin-num';
    numEl.textContent = pin.pin;

    const typeEl = document.createElement('div');
    typeEl.className = 'pin-type';
    typeEl.textContent = typeAbbrev[pin.type] || '';

    const nameEl = document.createElement('div');
    nameEl.className = 'pin-name';
    nameEl.textContent = pin.name ? truncate(pin.name, 8) : '';
    nameEl.title = pin.name || '';

    el.appendChild(numEl);
    el.appendChild(typeEl);
    el.appendChild(nameEl);

    el.addEventListener('click', () => openPinModal(pin.pin));

    return el;
}

function truncate(str, len) {
    return str.length > len ? str.substring(0, len - 1) + '...' : str;
}

// Render hardcoded devices
function renderHardcodedDevices() {
    if (!state.config) return;

    // Render bus addresses
    if (state.config.pccwdBus) {
        document.getElementById('of8wd-address').value = state.config.pccwdBus.of8wdAddress || 167;
        document.getElementById('in10wd-address').value = state.config.pccwdBus.in10wdAddress || 75;
    }

    // Render OF8Wd outputs
    const of8wdList = document.getElementById('of8wd-list');
    of8wdList.innerHTML = '';
    if (state.config.of8wd) {
        state.config.of8wd.forEach((device, index) => {
            of8wdList.appendChild(createDeviceItem('of8wd', index, device));
        });
    }

    // Render IN10Wd inputs
    const in10wdList = document.getElementById('in10wd-list');
    in10wdList.innerHTML = '';
    if (state.config.in10wd) {
        state.config.in10wd.forEach((device, index) => {
            in10wdList.appendChild(createDeviceItem('in10wd', index, device));
        });
    }

    // Render PWM LED
    const pwmLedList = document.getElementById('pwm-led-list');
    pwmLedList.innerHTML = '';
    if (state.config.pwmLed) {
        pwmLedList.appendChild(createDeviceItem('pwmLed', 0, state.config.pwmLed));
    }
}

function createDeviceItem(type, index, device) {
    const el = document.createElement('div');
    el.className = 'device-item' + (device.enabled ? ' enabled' : '');

    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.id = `${type}-${index}-enabled`;
    checkbox.checked = device.enabled;
    checkbox.addEventListener('change', () => {
        if (type === 'pwmLed') {
            state.config.pwmLed.enabled = checkbox.checked;
        } else {
            state.config[type][index].enabled = checkbox.checked;
        }
        el.classList.toggle('enabled', checkbox.checked);
        updateDirtyState();
    });

    const label = document.createElement('label');
    label.htmlFor = checkbox.id;
    label.className = 'device-index';
    if (type === 'of8wd') {
        label.textContent = `Output ${index + 1}`;
    } else if (type === 'in10wd') {
        label.textContent = `Switch ${index + 1}`;
    } else {
        label.textContent = 'LED';
    }

    const nameInput = document.createElement('input');
    nameInput.type = 'text';
    nameInput.className = 'device-name';
    nameInput.value = device.name || '';
    nameInput.placeholder = 'Name';
    nameInput.maxLength = 32;
    nameInput.addEventListener('input', () => {
        if (type === 'pwmLed') {
            state.config.pwmLed.name = nameInput.value;
        } else {
            state.config[type][index].name = nameInput.value;
        }
        updateDirtyState();
    });

    el.appendChild(checkbox);
    el.appendChild(label);
    el.appendChild(nameInput);

    return el;
}

function updateSystemInfo(info) {
    if (!info) return;

    document.getElementById('ip').textContent = info.ip || '--';
    document.getElementById('mcp-status').textContent = info.mcpConnected ? 'OK' : 'Error';
    document.getElementById('mcp-status').className = info.mcpConnected ? 'status-ok' : 'status-error';

    const uptime = info.uptime || 0;
    const hours = Math.floor(uptime / 3600);
    const minutes = Math.floor((uptime % 3600) / 60);
    document.getElementById('uptime').textContent = `${hours}h ${minutes}m`;
}

function updateDirtyState() {
    const hasChanges = JSON.stringify(state.config) !== JSON.stringify(state.originalConfig);
    state.dirty = hasChanges;

    document.getElementById('btn-save').disabled = !hasChanges;
    document.getElementById('changes-indicator').classList.toggle('hidden', !hasChanges);
}

// Modal functions
function openPinModal(pinNum) {
    state.selectedPin = pinNum;
    const pin = state.config.pins[pinNum];

    document.getElementById('modal-pin-num').textContent = pinNum;
    document.getElementById('pin-type').value = pin.type;
    document.getElementById('pin-name').value = pin.name || '';
    // inverted=true means active LOW, inverted=false means active HIGH
    document.getElementById('pin-active-level').value = pin.inverted ? 'low' : 'high';
    document.getElementById('pin-cooldown').value = pin.cooldown || 0;
    document.getElementById('valve-type').value = pin.valveType || 0;

    updateModalFields(pin.type);
    document.getElementById('pin-modal').classList.add('show');
}

function closeModal() {
    document.getElementById('pin-modal').classList.remove('show');
    state.selectedPin = null;
}

function updateModalFields(type) {
    const isInput = ['motion', 'leak', 'smoke', 'co2'].includes(type);
    const isValve = type === 'valve';
    const isUnused = type === 'unused';

    document.getElementById('inverted-group').classList.toggle('hidden', !isInput);
    document.getElementById('cooldown-group').classList.toggle('hidden', !isInput);
    document.getElementById('valve-type-group').classList.toggle('hidden', !isValve);
    document.getElementById('pin-name').disabled = isUnused;

    if (isUnused) {
        document.getElementById('pin-name').value = '';
        document.getElementById('pin-cooldown').value = 0;
    }
}

function applyPinConfig() {
    if (state.selectedPin === null) return;

    const pin = state.config.pins[state.selectedPin];
    pin.type = document.getElementById('pin-type').value;
    pin.name = document.getElementById('pin-name').value;
    // active LOW = inverted true, active HIGH = inverted false
    pin.inverted = document.getElementById('pin-active-level').value === 'low';
    pin.cooldown = parseInt(document.getElementById('pin-cooldown').value) || 0;
    pin.valveType = parseInt(document.getElementById('valve-type').value) || 0;

    renderPinGrid();
    updateDirtyState();
    closeModal();
}

// Error handling
function showError(message) {
    alert(message); // Simple error display, could be improved
}

// Event handlers
async function handleRefresh() {
    const config = await fetchConfig();
    if (config) {
        state.config = config;
        state.originalConfig = JSON.parse(JSON.stringify(config));
        renderPinGrid();
        renderHardcodedDevices();
        updateDirtyState();
    }

    const info = await fetchSystemInfo();
    updateSystemInfo(info);
}

async function handleSave() {
    if (!state.dirty) return;

    const btn = document.getElementById('btn-save');
    btn.disabled = true;
    btn.textContent = 'Saving...';

    const result = await saveConfig(state.config);
    if (result && result.success) {
        btn.textContent = 'Restarting...';
        await restartDevice();

        // Wait for restart and redirect
        setTimeout(() => {
            window.location.reload();
        }, 5000);
    } else {
        btn.disabled = false;
        btn.textContent = 'Save & Restart';
    }
}

// WebSocket Terminal functions
function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    // WebSocket is on port 81 (separate from HTTP on port 80)
    const wsUrl = `${protocol}//${window.location.hostname}:81/`;

    state.ws = new WebSocket(wsUrl);

    state.ws.onopen = () => {
        state.wsConnected = true;
        appendToTerminal('Connected to HomeSpan CLI\r\n', 'system');
    };

    state.ws.onclose = () => {
        state.wsConnected = false;
        appendToTerminal('\r\nDisconnected. Reconnecting...\r\n', 'system');
        setTimeout(connectWebSocket, 3000);
    };

    state.ws.onerror = (error) => {
        console.error('WebSocket error:', error);
    };

    state.ws.onmessage = (event) => {
        appendToTerminal(event.data);
    };
}

function appendToTerminal(text, type = 'output') {
    const output = document.getElementById('terminal-output');
    const span = document.createElement('span');
    span.textContent = text;
    if (type === 'system') {
        span.style.color = '#888';
    } else if (type === 'input') {
        span.style.color = '#fff';
    }
    output.appendChild(span);
    output.scrollTop = output.scrollHeight;

    // Limit terminal history
    while (output.childNodes.length > 500) {
        output.removeChild(output.firstChild);
    }
}

function sendCommand(cmd) {
    if (state.ws && state.wsConnected) {
        // Send command with newline
        state.ws.send(cmd + '\n');
    } else {
        appendToTerminal('Not connected\r\n', 'system');
    }
}

function handleTerminalInput(e) {
    if (e.key === 'Enter') {
        const input = document.getElementById('terminal-input');
        const cmd = input.value;
        if (cmd) {
            sendCommand(cmd);
            input.value = '';
        }
    }
}

// Initialize
document.addEventListener('DOMContentLoaded', async () => {
    // Set up event listeners
    document.getElementById('btn-refresh').addEventListener('click', handleRefresh);
    document.getElementById('btn-save').addEventListener('click', handleSave);
    document.getElementById('modal-close').addEventListener('click', closeModal);
    document.getElementById('btn-cancel').addEventListener('click', closeModal);
    document.getElementById('btn-apply').addEventListener('click', applyPinConfig);

    document.getElementById('pin-type').addEventListener('change', (e) => {
        updateModalFields(e.target.value);
    });

    // Close modal on backdrop click
    document.getElementById('pin-modal').addEventListener('click', (e) => {
        if (e.target.id === 'pin-modal') {
            closeModal();
        }
    });

    // Terminal event listeners
    document.getElementById('terminal-input').addEventListener('keydown', handleTerminalInput);
    document.getElementById('btn-send').addEventListener('click', () => {
        const input = document.getElementById('terminal-input');
        if (input.value) {
            sendCommand(input.value);
            input.value = '';
        }
    });

    // Quick command buttons
    document.querySelectorAll('.btn-cmd').forEach(btn => {
        btn.addEventListener('click', () => {
            const cmd = btn.dataset.cmd;
            sendCommand(cmd);
        });
    });

    // Bus address inputs
    document.getElementById('of8wd-address').addEventListener('change', (e) => {
        if (!state.config.pccwdBus) {
            state.config.pccwdBus = { of8wdAddress: 167, in10wdAddress: 75 };
        }
        state.config.pccwdBus.of8wdAddress = parseInt(e.target.value) || 167;
        updateDirtyState();
    });

    document.getElementById('in10wd-address').addEventListener('change', (e) => {
        if (!state.config.pccwdBus) {
            state.config.pccwdBus = { of8wdAddress: 167, in10wdAddress: 75 };
        }
        state.config.pccwdBus.in10wdAddress = parseInt(e.target.value) || 75;
        updateDirtyState();
    });

    // Initial load
    await handleRefresh();

    // Connect WebSocket for terminal
    connectWebSocket();
});
