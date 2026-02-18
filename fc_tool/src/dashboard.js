/**
 * dashboard.js — Live Dashboard Mode
 *
 * Toggleable panel showing key=value data updating in place.
 * Uses `=` separator only to avoid conflict with plotter's `:` format.
 */

const DASHBOARD_KV_RE = /([\w.]+)=(\S+)/g;

class Dashboard {
  constructor() {
    this.grid = document.getElementById("dashboard-grid");
    this.section = document.getElementById("dashboard-section");
    this.toggleBtn = document.getElementById("dashboard-toggle-btn");
    this.clearBtn = document.getElementById("dashboard-clear-btn");
    this.statusText = document.getElementById("dashboard-status-text");
    this.visible = false;
    this.values = new Map(); // key -> { value, el, flashTimer }

    this.toggleBtn.addEventListener("click", () => this.toggle());
    this.clearBtn.addEventListener("click", () => this.clear());
  }

  toggle() {
    this.visible = !this.visible;
    this.section.style.display = this.visible ? "" : "none";
    this.toggleBtn.textContent = this.visible ? "Hide Dashboard" : "Show Dashboard";
  }

  clear() {
    this.values.clear();
    this.grid.innerHTML = "";
    this.statusText.textContent = "Cleared";
  }

  /** Process raw serial data for key=value pairs. */
  processLine(data) {
    if (!this.visible) return;
    const lines = data.split("\n").filter(l => l.trim());
    let updated = false;

    for (const line of lines) {
      DASHBOARD_KV_RE.lastIndex = 0;
      let match;
      while ((match = DASHBOARD_KV_RE.exec(line)) !== null) {
        const key = match[1];
        const value = match[2];
        updated = true;

        if (this.values.has(key)) {
          const entry = this.values.get(key);
          if (entry.value !== value) {
            entry.value = value;
            entry.el.textContent = value;
            entry.el.classList.add("changed");
            clearTimeout(entry.flashTimer);
            entry.flashTimer = setTimeout(() => entry.el.classList.remove("changed"), 300);
          }
        } else {
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

          this.grid.appendChild(cell);
          this.values.set(key, { value, el: valEl, flashTimer: null });
        }
      }
    }

    if (updated) {
      this.statusText.textContent = `${this.values.size} key(s)`;
    }
  }
}

export { Dashboard };
