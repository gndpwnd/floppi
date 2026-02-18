/**
 * main.js — Application coordinator
 *
 * Imports modules and wires them together: connection, terminal, dashboard, plotter.
 */

import { PlotterManager } from './plotter.js';
import { parseAnsi } from './ansi.js';
import { Dashboard } from './dashboard.js';
import { Connection } from './connection.js';

const { invoke } = window.__TAURI__.core;
const { listen } = window.__TAURI__.event;

// ============================================================================
// Terminal State
// ============================================================================

const terminalOutput = document.getElementById("terminal-output");
const terminalInput = document.getElementById("terminal-input");
const autoscrollCheckbox = document.getElementById("autoscroll-checkbox");
const ansiCheckbox = document.getElementById("ansi-checkbox");
const filterInput = document.getElementById("filter-input");

const TERMINAL_MAX_LINES = 5000;
let activeFilter = "";
let userSelecting = false;

// Font size
const fontSizeDecBtn = document.getElementById("font-size-dec");
const fontSizeIncBtn = document.getElementById("font-size-inc");
const fontSizeLabel = document.getElementById("font-size-label");
const FONT_SIZE_MIN = 8;
const FONT_SIZE_MAX = 24;
let terminalFontSize = 12;

// ============================================================================
// Terminal Functions
// ============================================================================

function applyFilter(span) {
  if (activeFilter && !span.textContent.includes(activeFilter)) {
    span.classList.add("filtered");
  }
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
  dashboard.processLine(data);
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

function clearTerminal() {
  terminalOutput.innerHTML = "";
}

function setTerminalFontSize(size) {
  terminalFontSize = Math.max(FONT_SIZE_MIN, Math.min(FONT_SIZE_MAX, size));
  terminalOutput.style.fontSize = terminalFontSize + "px";
  fontSizeLabel.textContent = terminalFontSize;
}

// ============================================================================
// Modules
// ============================================================================

const dashboard = new Dashboard();
const conn = new Connection({ onSystem: appendSystem });

// ============================================================================
// Plotter
// ============================================================================

const plotter = new PlotterManager(document.getElementById("plotter-container"));
const plotterToggleBtn = document.getElementById("plotter-toggle-btn");
const plotterSection = document.getElementById("plotter-section");
const plotterPauseBtn = document.getElementById("plotter-pause-btn");
const plotterClearBtn = document.getElementById("plotter-clear-btn");
const plotterStatusText = document.getElementById("plotter-status-text");
const plotterWindowInput = document.getElementById("plotter-window-input");
const plotterPointsCheckbox = document.getElementById("plotter-points-checkbox");
const plotterKeepRecordingCheckbox = document.getElementById("plotter-keep-recording-checkbox");

let plotterVisible = false;
let totalSamples = 0;
let rateCounter = 0;
let lastRateTime = 0;
let currentRate = 0;
let rateStaleTimer = null;

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
// Event Listeners
// ============================================================================

// Connection
document.getElementById("scan-btn").addEventListener("click", () => conn.scanPorts());
document.getElementById("connect-btn").addEventListener("click", () => conn.toggleConnection());
document.getElementById("send-btn").addEventListener("click", async () => {
  const text = terminalInput.value;
  if (!text) return;
  if (await conn.sendData(text)) {
    appendTx(text);
    terminalInput.value = "";
  }
});
terminalInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !terminalInput.disabled) {
    document.getElementById("send-btn").click();
  }
});

// Terminal controls
document.getElementById("clear-monitor-btn").addEventListener("click", clearTerminal);
terminalOutput.addEventListener("mousedown", () => { userSelecting = true; });
document.addEventListener("mouseup", () => { userSelecting = false; });

filterInput.addEventListener("input", () => {
  activeFilter = filterInput.value;
  for (const span of terminalOutput.querySelectorAll("span")) {
    if (span.classList.contains("system")) continue;
    if (activeFilter && !span.textContent.includes(activeFilter)) {
      span.classList.add("filtered");
    } else {
      span.classList.remove("filtered");
    }
  }
});

document.getElementById("copy-monitor-btn").addEventListener("click", async () => {
  const btn = document.getElementById("copy-monitor-btn");
  try {
    await navigator.clipboard.writeText(terminalOutput.textContent);
    btn.textContent = "Copied!";
    setTimeout(() => { btn.textContent = "Copy All"; }, 1500);
  } catch {
    appendSystem("Clipboard copy failed");
  }
});

fontSizeDecBtn.addEventListener("click", () => setTerminalFontSize(terminalFontSize - 1));
fontSizeIncBtn.addEventListener("click", () => setTerminalFontSize(terminalFontSize + 1));

// Plotter controls
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
  if (val >= 50 && val <= 5000) plotter.maxDataPoints = val;
});

plotterPointsCheckbox.addEventListener("change", () => {
  plotter.setShowPoints(plotterPointsCheckbox.checked);
});

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
  if (e.ctrlKey && e.key === "l") {
    e.preventDefault();
    clearTerminal();
  }
  if (e.ctrlKey && e.shiftKey && e.key === "P") {
    e.preventDefault();
    plotterToggleBtn.click();
  }
  if (e.ctrlKey && e.shiftKey && e.key === " ") {
    e.preventDefault();
    if (plotterVisible) plotterPauseBtn.click();
  }
  if (e.ctrlKey && e.shiftKey && e.key === "D") {
    e.preventDefault();
    dashboard.toggle();
  }
  if (e.ctrlKey && !e.shiftKey && e.key === "f") {
    e.preventDefault();
    filterInput.focus();
    filterInput.select();
  }
  if (e.key === "Escape" && document.activeElement === filterInput) {
    filterInput.value = "";
    filterInput.dispatchEvent(new Event("input"));
    filterInput.blur();
  }
});

// ============================================================================
// Tauri Events
// ============================================================================

listen("serial-data", (event) => {
  appendRx(event.payload.data);
});

listen("serial-error", (event) => {
  appendSystem(`Error: ${event.payload.error}`);
});

listen("serial-disconnected", (event) => {
  conn._setConnected(false);
  appendSystem(`Device disconnected: ${event.payload.port}`);
  conn.startReconnect(event.payload.port);
});

listen("ports-changed", (event) => {
  const { added, removed } = event.payload;
  for (const p of added) appendSystem(`USB device connected: ${p}`);
  for (const p of removed) appendSystem(`USB device removed: ${p}`);
  // Auto-refresh port list if not currently connected
  if (!conn.isConnected) conn.scanPorts();
});

// ============================================================================
// Startup
// ============================================================================

conn.scanPorts().then(() => conn.applyStartupArgs());
