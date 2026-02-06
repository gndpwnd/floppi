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
const clearBtn = document.getElementById("clear-btn");
const newlineCheckbox = document.getElementById("newline-checkbox");
const autoscrollCheckbox = document.getElementById("autoscroll-checkbox");

// DOM Elements - IMU
const imuEnabledCheckbox = document.getElementById("imu-enabled-checkbox");
const imuStatus = document.getElementById("imu-status");

let isConnected = false;

// ============================================================================
// IMU Charts Configuration
// ============================================================================

const MAX_DATA_POINTS = 100; // ~10 seconds at 10Hz
const CHART_COLORS = {
  x: { border: "#ef5350", background: "#ef535040" }, // Red
  y: { border: "#81c784", background: "#81c78440" }, // Green
  z: { border: "#4fc3f7", background: "#4fc3f740" }, // Blue
};

// Chart.js default options for dark theme
const chartOptions = {
  responsive: true,
  maintainAspectRatio: false,
  animation: false, // Disable for real-time performance
  scales: {
    x: {
      display: false,
    },
    y: {
      grid: { color: "#333" },
      ticks: { color: "#888" },
    },
  },
  plugins: {
    legend: {
      labels: { color: "#888", boxWidth: 12, padding: 8 },
    },
  },
};

// Initialize accelerometer chart
const accelCtx = document.getElementById("accel-chart").getContext("2d");
const accelChart = new Chart(accelCtx, {
  type: "line",
  data: {
    labels: [],
    datasets: [
      {
        label: "X",
        data: [],
        borderColor: CHART_COLORS.x.border,
        backgroundColor: CHART_COLORS.x.background,
        borderWidth: 1.5,
        pointRadius: 0,
        tension: 0.2,
      },
      {
        label: "Y",
        data: [],
        borderColor: CHART_COLORS.y.border,
        backgroundColor: CHART_COLORS.y.background,
        borderWidth: 1.5,
        pointRadius: 0,
        tension: 0.2,
      },
      {
        label: "Z",
        data: [],
        borderColor: CHART_COLORS.z.border,
        backgroundColor: CHART_COLORS.z.background,
        borderWidth: 1.5,
        pointRadius: 0,
        tension: 0.2,
      },
    ],
  },
  options: chartOptions,
});

// Initialize gyroscope chart
const gyroCtx = document.getElementById("gyro-chart").getContext("2d");
const gyroChart = new Chart(gyroCtx, {
  type: "line",
  data: {
    labels: [],
    datasets: [
      {
        label: "X",
        data: [],
        borderColor: CHART_COLORS.x.border,
        backgroundColor: CHART_COLORS.x.background,
        borderWidth: 1.5,
        pointRadius: 0,
        tension: 0.2,
      },
      {
        label: "Y",
        data: [],
        borderColor: CHART_COLORS.y.border,
        backgroundColor: CHART_COLORS.y.background,
        borderWidth: 1.5,
        pointRadius: 0,
        tension: 0.2,
      },
      {
        label: "Z",
        data: [],
        borderColor: CHART_COLORS.z.border,
        backgroundColor: CHART_COLORS.z.background,
        borderWidth: 1.5,
        pointRadius: 0,
        tension: 0.2,
      },
    ],
  },
  options: chartOptions,
});

let imuSampleCount = 0;

// ============================================================================
// IMU Data Parsing
// ============================================================================

// Telemetry format patterns (configurable)
// Format 1: "IMU: ax=1.23 ay=4.56 az=7.89 gx=0.12 gy=0.34 gz=0.56"
// Format 2: "A:1.23,4.56,7.89 G:0.12,0.34,0.56"
// Format 3: JSON: {"accel":[1.23,4.56,7.89],"gyro":[0.12,0.34,0.56]}

const IMU_PATTERNS = [
  // Pattern 1: Key-value format
  /ax[=:]?\s*([-\d.]+).*?ay[=:]?\s*([-\d.]+).*?az[=:]?\s*([-\d.]+).*?gx[=:]?\s*([-\d.]+).*?gy[=:]?\s*([-\d.]+).*?gz[=:]?\s*([-\d.]+)/i,
  // Pattern 2: Compact format "A:x,y,z G:x,y,z"
  /A[=:]?\s*([-\d.]+)[,\s]+([-\d.]+)[,\s]+([-\d.]+).*?G[=:]?\s*([-\d.]+)[,\s]+([-\d.]+)[,\s]+([-\d.]+)/i,
  // Pattern 3: Simple CSV "ax,ay,az,gx,gy,gz"
  /^([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)$/,
];

function parseImuData(line) {
  // Try JSON first
  try {
    const json = JSON.parse(line);
    if (json.accel && json.gyro) {
      return {
        accel: { x: json.accel[0], y: json.accel[1], z: json.accel[2] },
        gyro: { x: json.gyro[0], y: json.gyro[1], z: json.gyro[2] },
      };
    }
    if (json.ax !== undefined) {
      return {
        accel: { x: json.ax, y: json.ay, z: json.az },
        gyro: { x: json.gx, y: json.gy, z: json.gz },
      };
    }
  } catch {
    // Not JSON, try regex patterns
  }

  // Try regex patterns
  for (const pattern of IMU_PATTERNS) {
    const match = line.match(pattern);
    if (match) {
      return {
        accel: {
          x: parseFloat(match[1]),
          y: parseFloat(match[2]),
          z: parseFloat(match[3]),
        },
        gyro: {
          x: parseFloat(match[4]),
          y: parseFloat(match[5]),
          z: parseFloat(match[6]),
        },
      };
    }
  }

  return null;
}

function updateCharts(imuData) {
  const timestamp = imuSampleCount++;

  // Update accelerometer chart
  accelChart.data.labels.push(timestamp);
  accelChart.data.datasets[0].data.push(imuData.accel.x);
  accelChart.data.datasets[1].data.push(imuData.accel.y);
  accelChart.data.datasets[2].data.push(imuData.accel.z);

  // Update gyroscope chart
  gyroChart.data.labels.push(timestamp);
  gyroChart.data.datasets[0].data.push(imuData.gyro.x);
  gyroChart.data.datasets[1].data.push(imuData.gyro.y);
  gyroChart.data.datasets[2].data.push(imuData.gyro.z);

  // Trim old data
  if (accelChart.data.labels.length > MAX_DATA_POINTS) {
    accelChart.data.labels.shift();
    accelChart.data.datasets.forEach((ds) => ds.data.shift());
    gyroChart.data.labels.shift();
    gyroChart.data.datasets.forEach((ds) => ds.data.shift());
  }

  // Update charts
  accelChart.update("none");
  gyroChart.update("none");

  // Update status
  imuStatus.textContent = `Samples: ${imuSampleCount} | A: ${imuData.accel.x.toFixed(2)}, ${imuData.accel.y.toFixed(2)}, ${imuData.accel.z.toFixed(2)}`;
}

function processSerialData(data) {
  // Only parse if IMU parsing is enabled
  if (!imuEnabledCheckbox.checked) return;

  // Split by newlines in case multiple lines arrive at once
  const lines = data.split("\n").filter((line) => line.trim());

  for (const line of lines) {
    const imuData = parseImuData(line.trim());
    if (imuData) {
      updateCharts(imuData);
    }
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
      option.textContent = `${p.name} - ${p.port_type}`;
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
    setConnected(true, port);
    appendSystem(`Connected to ${port} at ${baud} baud`);
    imuStatus.textContent = "Waiting for data...";
    imuSampleCount = 0;
  } catch (e) {
    appendSystem(`Connection failed: ${e}`);
  }
}

async function disconnect() {
  try {
    await invoke("close_serial_port");
    setConnected(false);
    appendSystem("Disconnected");
  } catch (e) {
    appendSystem(`Disconnect failed: ${e}`);
  }
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
// Terminal Functions
// ============================================================================

function appendRx(data) {
  const span = document.createElement("span");
  span.className = "rx";
  span.textContent = data;
  terminalOutput.appendChild(span);
  autoScroll();

  // Process for IMU data
  processSerialData(data);
}

function appendTx(data) {
  const span = document.createElement("span");
  span.className = "tx";
  span.textContent = `> ${data}\n`;
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

function autoScroll() {
  if (autoscrollCheckbox.checked) {
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

clearBtn.addEventListener("click", clearTerminal);

// Listen for serial data events from Tauri backend
listen("serial-data", (event) => {
  appendRx(event.payload.data);
});

listen("serial-error", (event) => {
  appendSystem(`Error: ${event.payload.error}`);
});

// Initial scan on load
scanPorts();
