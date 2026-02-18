import { PlotterManager } from './plotter.js';

const { invoke } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;

// DOM Elements - Connection
const portSelect = document.getElementById("port-select");
const baudSelect = document.getElementById("baud-select");
const scanBtn = document.getElementById("scan-btn");
const connectBtn = document.getElementById("connect-btn");
const connectionStatus = document.getElementById("connection-status");

// DOM Elements - Terminal
const terminalOutput = document.getElementById("terminal-output");
const terminalInput = document.getElementById("terminal-input");
const sendBtn = document.getElementById("send-btn");
const clearMonitorBtn = document.getElementById("clear-monitor-btn");
const copyMonitorBtn = document.getElementById("copy-monitor-btn");
const newlineCheckbox = document.getElementById("newline-checkbox");
const autoscrollCheckbox = document.getElementById("autoscroll-checkbox");
const filterInput = document.getElementById("filter-input");

// DOM Elements - Plotter
const plotterToggleBtn = document.getElementById("plotter-toggle-btn");
const plotterSection = document.getElementById("plotter-section");
const plotterPauseBtn = document.getElementById("plotter-pause-btn");
const plotterClearBtn = document.getElementById("plotter-clear-btn");
const plotterStatusText = document.getElementById("plotter-status-text");
const plotterWindowInput = document.getElementById("plotter-window-input");

let isConnected = false;
let lastPort = null;
let lastBaud = null;
let reconnectTimer = null;
let reconnectAttempts = 0;
const RECONNECT_INTERVAL_MS = 2000;
const RECONNECT_MAX_ATTEMPTS = 15;
const TERMINAL_MAX_LINES = 5000;
let activeFilter = "";
let userSelecting = false;  // Pause autoscroll during text selection
const ansiCheckbox = document.getElementById("ansi-checkbox");

// Font size controls
const fontSizeDecBtn = document.getElementById("font-size-dec");
const fontSizeIncBtn = document.getElementById("font-size-inc");
const fontSizeLabel = document.getElementById("font-size-label");
const FONT_SIZE_MIN = 8;
const FONT_SIZE_MAX = 24;
let terminalFontSize = 12;

// Plotter enhancement controls
const plotterPointsCheckbox = document.getElementById("plotter-points-checkbox");
const plotterKeepRecordingCheckbox = document.getElementById("plotter-keep-recording-checkbox");

// DOM Elements - Dashboard
const dashboardToggleBtn = document.getElementById("dashboard-toggle-btn");
const dashboardSection = document.getElementById("dashboard-section");
const dashboardGrid = document.getElementById("dashboard-grid");
const dashboardClearBtn = document.getElementById("dashboard-clear-btn");
const dashboardStatusText = document.getElementById("dashboard-status-text");

// ============================================================================
// Live Dashboard
// ============================================================================

let dashboardVisible = false;
const dashboardValues = new Map(); // key -> { value, el }

const DASHBOARD_KV_RE = /([\w.]+)=(\S+)/g;

dashboardToggleBtn.addEventListener("click", () => {
  dashboardVisible = !dashboardVisible;
  dashboardSection.style.display = dashboardVisible ? "" : "none";
  dashboardToggleBtn.textContent = dashboardVisible ? "Hide Dashboard" : "Show Dashboard";
});

dashboardClearBtn.addEventListener("click", () => {
  dashboardValues.clear();
  dashboardGrid.innerHTML = "";
  dashboardStatusText.textContent = "Cleared";
});

function processSerialForDashboard(data) {
  if (!dashboardVisible) return;
  const lines = data.split("\n").filter(l => l.trim());
  let updated = false;

  for (const line of lines) {
    DASHBOARD_KV_RE.lastIndex = 0;
    let match;
    while ((match = DASHBOARD_KV_RE.exec(line)) !== null) {
      const key = match[1];
      const value = match[2];
      updated = true;

      if (dashboardValues.has(key)) {
        const entry = dashboardValues.get(key);
        if (entry.value !== value) {
          entry.value = value;
          entry.el.textContent = value;
          // Brief highlight on change
          entry.el.classList.add("changed");
          clearTimeout(entry.flashTimer);
          entry.flashTimer = setTimeout(() => entry.el.classList.remove("changed"), 300);
        }
      } else {
        // Create new cell
        const cell = document.createElement("div");
        cell.className = "dashboard-cell";

        const keyEl = document.createElement("span");
        keyEl.className = "dashboard-key";
        keyEl.textContent = key;
        cell.appendChild(keyEl);

        const valEl = document.createElement("span");
        valEl.className = "dashboard-value";
        valEl.textContent = value;
        cell.appendChild(valEl);

        dashboardGrid.appendChild(cell);
        dashboardValues.set(key, { value, el: valEl, flashTimer: null });
      }
    }
  }

  if (updated) {
    dashboardStatusText.textContent = `${dashboardValues.size} key(s)`;
  }
}

// ============================================================================
// Plotter
// ============================================================================

const plotter = new PlotterManager(
  document.getElementById("plotter-container"),
);

let plotterVisible = false;
let totalSamples = 0;
let rateCounter = 0;
let lastRateTime = 0;
let currentRate = 0;
let rateStaleTimer = null;

plotterToggleBtn.addEventListener("click", () => {
  plotterVisible = !plotterVisible;
  plotterSection.style.display = plotterVisible ? "" : "none";
  plotterToggleBtn.textContent = plotterVisible ? "Hide Plotter" : "Show Plotter";
  plotter.enabled = plotterVisible;
});

plotterPauseBtn.addEventListener("click", () => {
  const paused = plotter.togglePause();
  plotterPauseBtn.textContent = paused ? "\u25B6" : "\u23F8";
  plotterPauseBtn.title = paused ? "Resume" : "Pause";
});

plotterClearBtn.addEventListener("click", () => {
  plotter.clearAll();
  totalSamples = 0;
  rateCounter = 0;
  currentRate = 0;
  plotterStatusText.textContent = "Cleared";
});

plotterWindowInput.addEventListener("change", () => {
  const val = parseInt(plotterWindowInput.value, 10);
  if (val >= 50 && val <= 5000) {
    plotter.maxDataPoints = val;
  }
});

function processSerialForPlotter(data) {
  if (!plotterVisible) return;
  const lines = data.split("\n").filter(l => l.trim());
  for (const line of lines) {
    plotter.processLine(line.trim());
  }
  totalSamples += lines.length;
  rateCounter += lines.length;

  const now = performance.now();
  if (now - lastRateTime >= 1000) {
    currentRate = rateCounter;
    rateCounter = 0;
    lastRateTime = now;
  }

  // Reset Hz display to 0 if data stops for 2 seconds
  clearTimeout(rateStaleTimer);
  rateStaleTimer = setTimeout(() => {
    currentRate = 0;
    rateCounter = 0;
    plotterStatusText.textContent =
      `${plotter.plots.size} plot(s) | ${totalSamples} samples`;
  }, 2000);

  if (totalSamples % 10 < lines.length) {
    const rateStr = currentRate > 0 ? ` | ${currentRate} Hz` : "";
    plotterStatusText.textContent =
      `${plotter.plots.size} plot(s) | ${totalSamples} samples${rateStr}`;
  }
}

// ============================================================================
// Serial Port Functions
// ============================================================================

async function scanPorts() {
  portSelect.disabled = true;
  portSelect.innerHTML = '<option value="">Scanning...</option>';

  try {
    const ports = await invoke("list_serial_ports");
    portSelect.innerHTML = "";

    if (ports.length === 0) {
      portSelect.innerHTML = '<option value="">No ports found</option>';
      connectBtn.disabled = true;
      return;
    }

    ports.forEach((p) => {
      const option = document.createElement("option");
      option.value = p.name;
      if (p.board_name) {
        option.textContent = `${p.name} — ${p.board_name} — ${p.port_type}`;
      } else {
        option.textContent = `${p.name} — ${p.port_type}`;
      }
      portSelect.appendChild(option);
    });

    portSelect.disabled = false;
    connectBtn.disabled = false;
  } catch (e) {
    portSelect.innerHTML = `<option value="">Error: ${e}</option>`;
    connectBtn.disabled = true;
  }
}

async function connect() {
  const port = portSelect.value;
  const baud = parseInt(baudSelect.value, 10);

  if (!port) {
    appendSystem("No port selected");
    return;
  }

  try {
    await invoke("open_serial_port", { portName: port, baudRate: baud });
    lastPort = port;
    lastBaud = baud;
    stopReconnect();
    setConnected(true, port);
    appendSystem(`Connected to ${port} at ${baud} baud`);
    totalSamples = 0;
  } catch (e) {
    appendSystem(`Connection failed: ${e}`);
  }
}

async function disconnect() {
  stopReconnect();
  try {
    await invoke("close_serial_port");
    setConnected(false);
    appendSystem("Disconnected");
  } catch (e) {
    appendSystem(`Disconnect failed: ${e}`);
  }
}

// ============================================================================
// Auto-Reconnect
// ============================================================================

function stopReconnect() {
  if (reconnectTimer) {
    clearInterval(reconnectTimer);
    reconnectTimer = null;
    reconnectAttempts = 0;
  }
}

function startReconnect(port) {
  if (reconnectTimer) return;
  const targetPort = port || lastPort;
  const targetBaud = lastBaud || parseInt(baudSelect.value, 10);
  if (!targetPort) return;

  reconnectAttempts = 0;
  appendSystem(`Reconnecting to ${targetPort}...`);
  connectionStatus.textContent = "Reconnecting...";
  connectionStatus.className = "status-disconnected";

  reconnectTimer = setInterval(async () => {
    reconnectAttempts++;
    if (reconnectAttempts > RECONNECT_MAX_ATTEMPTS) {
      stopReconnect();
      appendSystem("Reconnect failed — device not found");
      return;
    }
    try {
      await invoke("open_serial_port", { portName: targetPort, baudRate: targetBaud });
      stopReconnect();
      lastPort = targetPort;
      lastBaud = targetBaud;
      setConnected(true, targetPort);
      appendSystem(`Reconnected to ${targetPort}`);
    } catch {
      // Port not available yet
    }
  }, RECONNECT_INTERVAL_MS);
}

function setConnected(connected, port = null) {
  isConnected = connected;

  if (connected) {
    connectionStatus.textContent = `Connected: ${port}`;
    connectionStatus.className = "status-connected";
    connectBtn.textContent = "Disconnect";
    portSelect.disabled = true;
    baudSelect.disabled = true;
    terminalInput.disabled = false;
    sendBtn.disabled = false;
  } else {
    connectionStatus.textContent = "Disconnected";
    connectionStatus.className = "status-disconnected";
    connectBtn.textContent = "Connect";
    portSelect.disabled = false;
    baudSelect.disabled = false;
    terminalInput.disabled = true;
    sendBtn.disabled = true;
  }
}

async function sendData() {
  let data = terminalInput.value;
  if (!data) return;

  if (newlineCheckbox.checked) {
    data += "\n";
  }

  try {
    await invoke("send_serial_data", { data });
    appendTx(terminalInput.value);
    terminalInput.value = "";
  } catch (e) {
    appendSystem(`Send failed: ${e}`);
  }
}

// ============================================================================
// ANSI Escape Code Parser
// ============================================================================

// SGR color code → CSS class mapping
const ANSI_COLORS = {
  30: 'ansi-black',   31: 'ansi-red',     32: 'ansi-green',   33: 'ansi-yellow',
  34: 'ansi-blue',    35: 'ansi-magenta',  36: 'ansi-cyan',    37: 'ansi-white',
  90: 'ansi-bright-black', 91: 'ansi-bright-red', 92: 'ansi-bright-green', 93: 'ansi-bright-yellow',
  94: 'ansi-bright-blue', 95: 'ansi-bright-magenta', 96: 'ansi-bright-cyan', 97: 'ansi-bright-white',
};

const ANSI_REGEX = /\x1b\[([0-9;]*)m/g;

function parseAnsi(text) {
  const fragment = document.createDocumentFragment();
  let lastIndex = 0;
  let bold = false, dim = false, underline = false, color = null;

  ANSI_REGEX.lastIndex = 0;
  let match;

  while ((match = ANSI_REGEX.exec(text)) !== null) {
    // Text before this escape sequence
    if (match.index > lastIndex) {
      const span = document.createElement('span');
      span.textContent = text.slice(lastIndex, match.index);
      if (bold) span.classList.add('ansi-bold');
      if (dim) span.classList.add('ansi-dim');
      if (underline) span.classList.add('ansi-underline');
      if (color) span.classList.add(color);
      fragment.appendChild(span);
    }
    lastIndex = match.index + match[0].length;

    // Parse SGR codes (e.g., "1;31" → bold + red)
    const codes = match[1] ? match[1].split(';').map(Number) : [0];
    for (const code of codes) {
      if (code === 0) { bold = false; dim = false; underline = false; color = null; }
      else if (code === 1) bold = true;
      else if (code === 2) dim = true;
      else if (code === 4) underline = true;
      else if (code === 22) { bold = false; dim = false; }
      else if (code === 24) underline = false;
      else if (code === 39) color = null;
      else if (ANSI_COLORS[code]) color = ANSI_COLORS[code];
    }
  }

  // Remaining text after last escape
  if (lastIndex < text.length) {
    const span = document.createElement('span');
    span.textContent = text.slice(lastIndex);
    if (bold) span.classList.add('ansi-bold');
    if (dim) span.classList.add('ansi-dim');
    if (underline) span.classList.add('ansi-underline');
    if (color) span.classList.add(color);
    fragment.appendChild(span);
  }

  return fragment;
}

// ============================================================================
// Terminal Functions
// ============================================================================

function applyFilter(span) {
  if (activeFilter && !span.textContent.includes(activeFilter)) {
    span.classList.add("filtered");
  }
}

function appendRx(data) {
  const container = document.createElement("span");
  container.className = "rx";

  if (ansiCheckbox.checked && data.includes('\x1b[')) {
    container.appendChild(parseAnsi(data));
  } else {
    container.textContent = data;
  }

  applyFilter(container);
  terminalOutput.appendChild(container);
  trimTerminal();
  autoScroll();

  processSerialForPlotter(data);
  processSerialForDashboard(data);
}

function appendTx(data) {
  const span = document.createElement("span");
  span.className = "tx";
  span.textContent = `> ${data}\n`;
  applyFilter(span);
  terminalOutput.appendChild(span);
  autoScroll();
}

function appendSystem(msg) {
  const span = document.createElement("span");
  span.className = "system";
  span.textContent = `[${msg}]\n`;
  terminalOutput.appendChild(span);
  autoScroll();
}

function trimTerminal() {
  while (terminalOutput.childNodes.length > TERMINAL_MAX_LINES) {
    terminalOutput.removeChild(terminalOutput.firstChild);
  }
}

function autoScroll() {
  if (autoscrollCheckbox.checked && !userSelecting) {
    terminalOutput.scrollTop = terminalOutput.scrollHeight;
  }
}

function clearTerminal() {
  terminalOutput.innerHTML = "";
}

// ============================================================================
// Event Listeners
// ============================================================================

scanBtn.addEventListener("click", scanPorts);

connectBtn.addEventListener("click", () => {
  if (isConnected) {
    disconnect();
  } else {
    connect();
  }
});

sendBtn.addEventListener("click", sendData);

terminalInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !terminalInput.disabled) {
    sendData();
  }
});

clearMonitorBtn.addEventListener("click", clearTerminal);

// Pause autoscroll while user is selecting text (click-and-drag)
terminalOutput.addEventListener("mousedown", () => { userSelecting = true; });
document.addEventListener("mouseup", () => { userSelecting = false; });

filterInput.addEventListener("input", () => {
  activeFilter = filterInput.value;
  const spans = terminalOutput.querySelectorAll("span");
  for (const span of spans) {
    if (span.classList.contains("system")) continue;
    if (activeFilter && !span.textContent.includes(activeFilter)) {
      span.classList.add("filtered");
    } else {
      span.classList.remove("filtered");
    }
  }
});

copyMonitorBtn.addEventListener("click", async () => {
  const text = terminalOutput.textContent;
  try {
    await navigator.clipboard.writeText(text);
    copyMonitorBtn.textContent = "Copied!";
    setTimeout(() => { copyMonitorBtn.textContent = "Copy All"; }, 1500);
  } catch {
    appendSystem("Clipboard copy failed");
  }
});

// Font size controls
function setTerminalFontSize(size) {
  terminalFontSize = Math.max(FONT_SIZE_MIN, Math.min(FONT_SIZE_MAX, size));
  terminalOutput.style.fontSize = terminalFontSize + "px";
  fontSizeLabel.textContent = terminalFontSize;
}

fontSizeDecBtn.addEventListener("click", () => setTerminalFontSize(terminalFontSize - 1));
fontSizeIncBtn.addEventListener("click", () => setTerminalFontSize(terminalFontSize + 1));

// Show data points toggle
plotterPointsCheckbox.addEventListener("change", () => {
  plotter.setShowPoints(plotterPointsCheckbox.checked);
});

// Keep recording while paused
plotterKeepRecordingCheckbox.addEventListener("change", () => {
  plotter.keepRecording = plotterKeepRecordingCheckbox.checked;
});

// GUI-mode logging
const logToggleBtn = document.getElementById("log-toggle-btn");
let logActive = false;

logToggleBtn.addEventListener("click", async () => {
  if (!logActive) {
    try {
      const path = await invoke("start_log", { filePath: null });
      logActive = true;
      logToggleBtn.textContent = `Log: ${path}`;
      logToggleBtn.classList.add("active");
      appendSystem(`Logging to ${path}`);
    } catch (e) {
      appendSystem(`Log failed: ${e}`);
    }
  } else {
    try {
      const path = await invoke("stop_log");
      logActive = false;
      logToggleBtn.textContent = "Log";
      logToggleBtn.classList.remove("active");
      appendSystem(`Log saved: ${path || "unknown"}`);
    } catch (e) {
      appendSystem(`Stop log failed: ${e}`);
    }
  }
});

// Keyboard shortcuts
document.addEventListener("keydown", (e) => {
  // Ctrl+L — clear terminal
  if (e.ctrlKey && e.key === "l") {
    e.preventDefault();
    clearTerminal();
  }
  // Ctrl+Shift+P — toggle plotter
  if (e.ctrlKey && e.shiftKey && e.key === "P") {
    e.preventDefault();
    plotterToggleBtn.click();
  }
  // Ctrl+Shift+Space — pause/resume plotter
  if (e.ctrlKey && e.shiftKey && e.key === " ") {
    e.preventDefault();
    if (plotterVisible) plotterPauseBtn.click();
  }
  // Ctrl+Shift+D — toggle dashboard
  if (e.ctrlKey && e.shiftKey && e.key === "D") {
    e.preventDefault();
    dashboardToggleBtn.click();
  }
  // Ctrl+F — focus filter input
  if (e.ctrlKey && !e.shiftKey && e.key === "f") {
    e.preventDefault();
    filterInput.focus();
    filterInput.select();
  }
  // Escape — clear filter and return focus
  if (e.key === "Escape" && document.activeElement === filterInput) {
    filterInput.value = "";
    filterInput.dispatchEvent(new Event("input"));
    filterInput.blur();
  }
});

// Listen for serial data events from Tauri backend
listen("serial-data", (event) => {
  appendRx(event.payload.data);
});

listen("serial-error", (event) => {
  appendSystem(`Error: ${event.payload.error}`);
});

listen("serial-disconnected", (event) => {
  setConnected(false);
  appendSystem(`Device disconnected: ${event.payload.port}`);
  startReconnect(event.payload.port);
});

// Initial scan on load, then check for CLI startup args
scanPorts().then(async () => {
  try {
    const args = await invoke("get_startup_args");
    if (args.port) {
      // Pre-select port in dropdown (add if not found from scan)
      let found = false;
      for (const opt of portSelect.options) {
        if (opt.value === args.port) {
          opt.selected = true;
          found = true;
          break;
        }
      }
      if (!found) {
        const opt = document.createElement("option");
        opt.value = args.port;
        opt.textContent = args.port;
        opt.selected = true;
        portSelect.appendChild(opt);
      }

      // Set baud rate if provided
      if (args.baud) {
        for (const opt of baudSelect.options) {
          if (parseInt(opt.value, 10) === args.baud) {
            opt.selected = true;
            break;
          }
        }
      }

      // Auto-connect
      appendSystem(`CLI: auto-connecting to ${args.port} @ ${args.baud || baudSelect.value} baud`);
      connect();
    }
  } catch {
    // No startup args or command not available — normal startup
  }
});
