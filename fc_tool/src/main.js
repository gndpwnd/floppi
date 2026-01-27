const { invoke } = window.__TAURI__.core;

document.getElementById("scan-btn").addEventListener("click", async () => {
  const portsList = document.getElementById("ports-list");
  portsList.innerHTML = "<p>Scanning...</p>";

  try {
    const ports = await invoke("list_serial_ports");
    if (ports.length === 0) {
      portsList.innerHTML = "<p>No serial ports found.</p>";
      return;
    }
    portsList.innerHTML = ports
      .map(
        (p) =>
          `<div class="port-item">
            <strong>${p.name}</strong>
            <span class="port-type">${p.port_type}</span>
          </div>`
      )
      .join("");
  } catch (e) {
    portsList.innerHTML = `<p class="error">Error: ${e}</p>`;
  }
});
