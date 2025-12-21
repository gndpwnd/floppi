Here’s a condensed, structured document you can add to your drone project documentation. I’ve organized it for clarity, including **findings, best practices, and “do’s and don’ts”** about powering your flight controller with a BEC.

---

# BEC (Battery Eliminator Circuit) Documentation for UAV Project

## 1. Overview

A **BEC (Battery Eliminator Circuit)** allows your RC electronics — typically the **flight controller, receiver, and servos** — to be powered **directly from the main LiPo battery**, eliminating the need for a separate electronics battery. It regulates the high voltage of the LiPo down to a **safe, consistent voltage**.

**Key principle:**

```
Battery (2–6S LiPo) → BEC → 5V (or 6–8V for high-voltage servos) → Flight Controller + Receiver + Sensors
```

---

## 2. Why BECs Are Needed

* ESCs power motors using the LiPo voltage (e.g., 7.4–22.2 V for 2–6S).
* Flight controllers and receivers require **5 V logic** (some sensors 3.3 V).
* A BEC **steps down** battery voltage safely.
* Avoid running multiple BECs in parallel unless properly isolated (never connect two BEC outputs together directly).

---

## 3. Types of BECs

| Type                         | Description                                                                                                         |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| **Linear (L-BEC)**           | Simple, inexpensive, voltage regulated via heat dissipation. Inefficient at high voltage; generates heat.           |
| **Switching (S-BEC / UBEC)** | Efficient, handles high currents and voltage ranges (2–6S). Most modern UAVs use these.                             |
| **Standalone UBEC**          | External BEC used when: high-current draw, ESC is opto (no BEC), or reliability is critical (aircraft, large UAVs). |
| **Opto ESC**                 | ESC has no BEC; requires a separate UBEC.                                                                           |

---

## 4. BEC Design Guidelines

### 4.1 Voltage and Current Requirements

* Flight controllers typically require **5 V logic**.
* Sensors + FC + RX can draw **300 mA – 1 A** (check your specific FC).
* Use a **UBEC rated ≥2–3 A** for safety and future expansion.

### 4.2 Avoid Linear 7805 Regulators

* L7805CV is **not suitable** for 2–6S LiPo:

  * **Heat dissipation** becomes excessive at higher voltages.
  * **Efficiency is poor** (22–40% for 3–6S).
  * **Brownouts** or thermal shutdown can reboot the FC mid-flight.
* Only acceptable in bench testing with **≤2S LiPo**, low current, and heatsinking.

### 4.3 Capacitors

* Place **470–1000 µF electrolytic** near FC input for voltage spikes.
* Optional **0.1 µF ceramic** across input and output pins to stabilize high-frequency noise.

---

## 5. Wiring Architecture

### 5.1 Big Picture

```
LiPo
 ├── ESCs (motor power)
 └── UBEC (5V regulated)
        ├── Flight Controller 5V
        ├── Receiver
        └── Sensors
```

### 5.2 Step-by-Step Connections

1. **LiPo → UBEC input**

   * VIN+ → LiPo +
   * GND → LiPo GND

2. **UBEC → Flight Controller**

   * UBEC +5V → FC 5V pin
   * UBEC GND → FC GND

3. **ESC connections**

   * Signal → FC PWM pins
   * GND → FC GND
   * **Remove or ignore ESC red wire** to avoid voltage conflicts

4. **Sensors**

   * 5V sensors → UBEC 5V
   * GND → shared ground

**Important:** All components **must share a common ground**.

---

## 6. Brownouts and Glitch Busters

* **Brownout:** occurs if a servo draws more current than the BEC can supply, causing voltage dips and potential FC reboot.
* **Glitch buster:** a capacitor placed on a spare receiver channel or near FC input acts as a short-term current reservoir to smooth out voltage spikes.

---

## 7. Recommendations

* **Use a switching UBEC (RC-rated)** for 2–6S LiPo inputs, rated for at least **2–3 A continuous**.
* Avoid linear regulators like the L7805CV except for bench-testing small 2S setups.
* Disconnect ESC BEC power if adding a standalone UBEC.
* Include bulk capacitors for transient voltage stabilization.
* Always verify voltage and current ratings for all peripherals.

**Suggested UBEC setup:**

* Input: 2–6S LiPo
* Output: 5V
* Max current: ≥3 A
* Output wires: Red → 5V, Black → GND, Yellow → ignored
* Parallel with LiPo, not in series with ESC

---

## 8. Summary Table

| Component  | Input Voltage | Output Voltage | Notes                                     |
| ---------- | ------------- | -------------- | ----------------------------------------- |
| FC / MCU   | 5 V           | N/A            | Powered by UBEC only                      |
| ESC (opto) | LiPo voltage  | N/A            | Bypass ESC BEC, use standalone UBEC       |
| UBEC       | 2–6S LiPo     | 5 V            | Switching type recommended                |
| Capacitors | N/A           | N/A            | 470–1000 µF electrolytic + 0.1 µF ceramic |

---

### Key Takeaways

* **UBEC is mandatory** for modern UAVs using 2–6S LiPo.
* **Never use L7805** for mid/high voltage LiPo.
* **Parallel connection to battery + proper grounding** ensures reliable FC operation.
* **Single regulated 5 V rail** prevents brownouts and damage.

---

If you want, I can also make a **schematic diagram showing LiPo → UBEC → Flight Controller + ESCs**, which would make this documentation even more practical for assembly.

Do you want me to make that diagram?
