# Arduino Serial Plotter Protocol Compatibility

> Date: 2026-02-18
> Context: Research for fc_tool serial protocol lexicon documentation

---

## Arduino IDE Serial Plotter Format Specification

The official protocol is documented in [ArduinoSerialPlotterProtocol.md](https://github.com/arduino/Arduino/blob/master/build/shared/ArduinoSerialPlotterProtocol.md).

### Line Structure

```text
Part1 <separator> Part2 <separator> ... PartN \n
```

Each "part" can be:

- **Label + Value:** `Label:Value` (e.g., `temp:25.3`)
- **Label only:** `Label:` (sets legend name without data point)
- **Value only:** `25.3` (auto-named by column index)

**Separators between parts:** space (` `), tab (`\t`), or comma (`,`)

**Label-value separator:** colon (`:`) with **no space** after colon

**Line terminator:** `\n`

### IDE 1.x vs 2.x Differences

| Feature | IDE 1.x (Java) | IDE 2.x (Electron/Web) |
| --- | --- | --- |
| Labels added | v1.8.10 (Sept 2019) | Inherited |
| Label format | `label:value` inline | Same |
| Header format | `label1:\tlabel2:\n` then values | **Broken** (first msg discarded, [issue #14](https://github.com/arduino/arduino-serial-plotter-webapp/issues/14)) |
| Separators | Space, tab, comma (`[, \t]+`) | Same, but labeled+tab/space was broken then [fixed](https://github.com/arduino/arduino-serial-plotter-webapp/issues/15) |
| Max label length | 32 characters | Not explicitly limited |
| Parser source | `SerialPlotter.java` | `arduino-serial-plotter-webapp` (TypeScript) |

The core protocol is identical across versions.

### Arduino IDE Parser Regex

From `SerialPlotter.java` (IDE 1.x):

```java
String[] parts = line.split("[, \\t]+");
if (parts[i].contains(":")) {
    String[] subString = parts[i].split("[:]+" );
    label = subString[0].substring(0, Math.min(labelLength, 32));
    parts[i] = subString[1];
}
value = Double.valueOf(parts[i]);
```

---

## fc_tool Compatibility Matrix

fc_tool's plotter regex: `/([\w.]+)(?:@(\d+))?[=:]([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)/g`

| Format | Example | Arduino IDE | fc_tool | Notes |
| --- | --- | --- | --- | --- |
| `name:value` | `temp:25.3` | Yes (v1.8.10+) | Yes | Fully compatible |
| `name@plotId:value` | `ax@0:1.02` | Partial | Yes | Arduino shows `ax@0` as label |
| `name=value` | `temp=25.3` | No | Yes | fc_tool extension |
| Plain CSV | `25.3,60.0` | Yes | Yes | Universal |
| Space-separated | `25.3 60.0` | Yes | Yes | Universal |
| Tab-separated | `25.3\t60.0` | Yes | Yes | Universal |
| Header then values | `a:\tb:\n 1\t2\n` | Yes (buggy in 2.x) | No | Not supported |
| Teleplot `>name:value` | `>sin:0.5` | No | No | `>` not in `\w` |

### Key Takeaways

1. **`name:value` is the universal format** — works in Arduino IDE, fc_tool, and most third-party plotters
2. **`name@plotId:value` degrades gracefully** — Arduino IDE shows `ax@0` as the label name (functional but ugly)
3. **Header format not supported** — fc_tool only does inline labels. This is fine since headers are buggy in Arduino IDE 2.x anyway
4. **Teleplot incompatible** — would need stripping `>` prefix (one-line change if desired)

---

## Third-Party Tool Comparison

### Teleplot (VS Code Extension)

Uses a different protocol with mandatory `>` prefix for serial data.

| Feature | Arduino Plotter | fc_tool | Teleplot |
| --- | --- | --- | --- |
| Format | `label:value` | `name@id:value` | `>label:value` |
| Prefix required | No | No | Yes (`>`) |
| Timestamps | Implicit | Implicit | Optional explicit |
| Units | No | No | Yes (`\xa7unit`) |
| XY plots | No | No | Yes |
| Text values | No | Dashboard only | Yes |
| Multi-graph | No (1 graph) | Yes (10 graphs) | Yes |
| Transport | Serial only | Serial only | Serial + UDP |

### Better Serial Plotter

Drop-in Arduino replacement. Identical `label:value` protocol. No prefix required.

---

## Arduino Code Examples

### Single value (no label)

```cpp
void loop() {
    Serial.println(analogRead(A0));
    delay(50);
}
```

### Multiple labeled values (recommended)

```cpp
void loop() {
    Serial.print("sin:"); Serial.print(sin(t));
    Serial.print(" cos:"); Serial.println(cos(t));
    t += 0.1;
    delay(50);
}
```

### fc_tool multi-graph (gracefully degrades in Arduino IDE)

```cpp
void loop() {
    Serial.print("ax@0:"); Serial.print(accel_x, 2);
    Serial.print(" ay@0:"); Serial.print(accel_y, 2);
    Serial.print(" temp@1:"); Serial.println(temperature, 1);
    delay(20);
}
```

---

## Sources

- [ArduinoSerialPlotterProtocol.md (Official)](https://github.com/arduino/Arduino/blob/master/build/shared/ArduinoSerialPlotterProtocol.md)
- [SerialPlotter.java (IDE 1.x Source)](https://github.com/arduino/Arduino/blob/master/app/src/processing/app/SerialPlotter.java)
- [Arduino IDE 2.x Serial Plotter Tutorial](https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-serial-plotter/)
- [Header labels broken in 2.x (Issue #14)](https://github.com/arduino/arduino-serial-plotter-webapp/issues/14)
- [Non-comma separator bug (Issue #15)](https://github.com/arduino/arduino-serial-plotter-webapp/issues/15)
- [Label feature history (Issue #4180)](https://github.com/arduino/Arduino/issues/4180)
- [Teleplot Protocol](https://github.com/nesnes/teleplot)
- [Better Serial Plotter](https://github.com/nathandunk/BetterSerialPlotter)
