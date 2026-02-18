/**
 * connection.js — Serial port connection management
 *
 * Handles scanning, connecting, disconnecting, auto-reconnect,
 * and sending data over serial.
 */

const { invoke } = window.__TAURI__.core;

const RECONNECT_INTERVAL_MS = 2000;
const RECONNECT_MAX_ATTEMPTS = 15;

class Connection {
  /**
   * @param {object} opts
   * @param {function} opts.onSystem - callback for system messages: (msg) => void
   */
  constructor(opts) {
    this.portSelect = document.getElementById("port-select");
    this.baudSelect = document.getElementById("baud-select");
    this.connectBtn = document.getElementById("connect-btn");
    this.connectionStatus = document.getElementById("connection-status");
    this.terminalInput = document.getElementById("terminal-input");
    this.sendBtn = document.getElementById("send-btn");
    this.newlineCheckbox = document.getElementById("newline-checkbox");

    this.onSystem = opts.onSystem || (() => {});

    this.connected = false;
    this.lastPort = null;
    this.lastBaud = null;
    this._reconnectTimer = null;
    this._reconnectAttempts = 0;
  }

  get isConnected() { return this.connected; }

  async scanPorts() {
    this.portSelect.disabled = true;
    this.portSelect.innerHTML = '<option value="">Scanning...</option>';

    try {
      const ports = await invoke("list_serial_ports");
      this.portSelect.innerHTML = "";

      if (ports.length === 0) {
        this.portSelect.innerHTML = '<option value="">No ports found</option>';
        this.connectBtn.disabled = true;
        return;
      }

      ports.forEach((p) => {
        const option = document.createElement("option");
        option.value = p.name;
        option.textContent = p.board_name
          ? `${p.name} — ${p.board_name} — ${p.port_type}`
          : `${p.name} — ${p.port_type}`;
        this.portSelect.appendChild(option);
      });

      this.portSelect.disabled = false;
      this.connectBtn.disabled = false;
    } catch (e) {
      this.portSelect.innerHTML = `<option value="">Error: ${e}</option>`;
      this.connectBtn.disabled = true;
    }
  }

  async connect() {
    const port = this.portSelect.value;
    const baud = parseInt(this.baudSelect.value, 10);
    if (!port) {
      this.onSystem("No port selected");
      return;
    }

    try {
      await invoke("open_serial_port", { portName: port, baudRate: baud });
      this.lastPort = port;
      this.lastBaud = baud;
      this._stopReconnect();
      this._setConnected(true, port);
      this.onSystem(`Connected to ${port} at ${baud} baud`);
    } catch (e) {
      this.onSystem(`Connection failed: ${e}`);
    }
  }

  async disconnect() {
    this._stopReconnect();
    try {
      await invoke("close_serial_port");
      this._setConnected(false);
      this.onSystem("Disconnected");
    } catch (e) {
      this.onSystem(`Disconnect failed: ${e}`);
    }
  }

  async toggleConnection() {
    if (this.connected) {
      await this.disconnect();
    } else {
      await this.connect();
    }
  }

  async sendData(text) {
    let data = text;
    if (this.newlineCheckbox.checked) data += "\n";

    try {
      await invoke("send_serial_data", { data });
      return true;
    } catch (e) {
      this.onSystem(`Send failed: ${e}`);
      return false;
    }
  }

  startReconnect(port) {
    if (this._reconnectTimer) return;
    const targetPort = port || this.lastPort;
    const targetBaud = this.lastBaud || parseInt(this.baudSelect.value, 10);
    if (!targetPort) return;

    this._reconnectAttempts = 0;
    this.onSystem(`Reconnecting to ${targetPort}...`);
    this.connectionStatus.textContent = "Reconnecting...";
    this.connectionStatus.className = "status-disconnected";

    this._reconnectTimer = setInterval(async () => {
      this._reconnectAttempts++;
      if (this._reconnectAttempts > RECONNECT_MAX_ATTEMPTS) {
        this._stopReconnect();
        this.onSystem("Reconnect failed — device not found");
        return;
      }
      try {
        await invoke("open_serial_port", { portName: targetPort, baudRate: targetBaud });
        this._stopReconnect();
        this.lastPort = targetPort;
        this.lastBaud = targetBaud;
        this._setConnected(true, targetPort);
        this.onSystem(`Reconnected to ${targetPort}`);
      } catch {
        // Port not available yet
      }
    }, RECONNECT_INTERVAL_MS);
  }

  /** Apply CLI startup args (auto-select port/baud and connect). */
  async applyStartupArgs() {
    try {
      const args = await invoke("get_startup_args");
      if (!args.port) return;

      let found = false;
      for (const opt of this.portSelect.options) {
        if (opt.value === args.port) { opt.selected = true; found = true; break; }
      }
      if (!found) {
        const opt = document.createElement("option");
        opt.value = args.port;
        opt.textContent = args.port;
        opt.selected = true;
        this.portSelect.appendChild(opt);
      }
      if (args.baud) {
        for (const opt of this.baudSelect.options) {
          if (parseInt(opt.value, 10) === args.baud) { opt.selected = true; break; }
        }
      }
      this.onSystem(`CLI: auto-connecting to ${args.port} @ ${args.baud || this.baudSelect.value} baud`);
      await this.connect();
    } catch {
      // No startup args — normal startup
    }
  }

  _stopReconnect() {
    if (this._reconnectTimer) {
      clearInterval(this._reconnectTimer);
      this._reconnectTimer = null;
      this._reconnectAttempts = 0;
    }
  }

  _setConnected(connected, port = null) {
    this.connected = connected;
    if (connected) {
      this.connectionStatus.textContent = `Connected: ${port}`;
      this.connectionStatus.className = "status-connected";
      this.connectBtn.textContent = "Disconnect";
      this.portSelect.disabled = true;
      this.baudSelect.disabled = true;
      this.terminalInput.disabled = false;
      this.sendBtn.disabled = false;
    } else {
      this.connectionStatus.textContent = "Disconnected";
      this.connectionStatus.className = "status-disconnected";
      this.connectBtn.textContent = "Connect";
      this.portSelect.disabled = false;
      this.baudSelect.disabled = false;
      this.terminalInput.disabled = true;
      this.sendBtn.disabled = true;
    }
  }
}

export { Connection };
