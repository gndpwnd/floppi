# Chart.js Oscilloscope Features Research

> Last updated: 2026-02-06

Research on implementing oscilloscope-like features with Chart.js for fc_tool's enhanced plotter.

---

## Summary

Chart.js can support oscilloscope-style features through plugins and custom implementations:

| Feature | Solution | Complexity |
|---------|----------|------------|
| Crosshair cursor | chartjs-plugin-crosshair | Low |
| Marker lines | chartjs-plugin-annotation | Low |
| Pause/buffer | Custom implementation | Medium |
| Log scale | Built-in `type: 'logarithmic'` | Low |
| Zoom | chartjs-plugin-zoom | Low |
| Mouse hover | Custom tooltip callbacks | Low |

---

## 1. Measurement Cursors

### chartjs-plugin-crosshair

Provides real-time crosshair following mouse movement:

```javascript
import CrosshairPlugin from 'chartjs-plugin-crosshair';
Chart.register(CrosshairPlugin);

const chart = new Chart(ctx, {
  type: 'line',
  options: {
    plugins: {
      crosshair: {
        line: {
          color: '#F66',
          width: 1,
          dashPattern: [5, 5]
        },
        sync: {
          enabled: false
        },
        zoom: {
          enabled: true
        }
      }
    }
  }
});
```

### chartjs-plugin-annotation

For static threshold/reference lines:

```javascript
import annotationPlugin from 'chartjs-plugin-annotation';
Chart.register(annotationPlugin);

const chart = new Chart(ctx, {
  options: {
    plugins: {
      annotation: {
        annotations: {
          thresholdLine: {
            type: 'line',
            yMin: 100,
            yMax: 100,
            borderColor: 'rgb(255, 99, 132)',
            borderWidth: 2,
            borderDash: [5, 5],
            label: {
              content: 'Threshold',
              enabled: true
            }
          }
        }
      }
    }
  }
});
```

---

## 2. Pause/Freeze Mode

Custom implementation that buffers data while paused:

```javascript
class PauseableChart {
  constructor(chart) {
    this.chart = chart;
    this.buffer = [];
    this.isPaused = false;
    this.maxBuffer = 1000;
  }

  addData(label, value) {
    if (this.isPaused) {
      this.buffer.push({ label, value });
      if (this.buffer.length > this.maxBuffer) {
        this.buffer.shift();  // Drop oldest
      }
    } else {
      this._pushToChart(label, value);
    }
  }

  _pushToChart(label, value) {
    this.chart.data.labels.push(label);
    this.chart.data.datasets[0].data.push(value);

    // Keep window size
    if (this.chart.data.labels.length > 100) {
      this.chart.data.labels.shift();
      this.chart.data.datasets[0].data.shift();
    }

    this.chart.update('quiet');
  }

  pause() {
    this.isPaused = true;
  }

  resume() {
    this.isPaused = false;
    // Flush buffer
    this.buffer.forEach(({ label, value }) => {
      this._pushToChart(label, value);
    });
    this.buffer = [];
  }

  getBufferSize() {
    return this.buffer.length;
  }
}
```

---

## 3. Mouse Hover Values

Custom tooltip with coordinates:

```javascript
const chart = new Chart(ctx, {
  options: {
    plugins: {
      tooltip: {
        enabled: true,
        callbacks: {
          title: (items) => `Sample ${items[0].dataIndex}`,
          label: (context) => {
            const x = context.parsed.x;
            const y = context.parsed.y;
            return [
              `X: ${x.toFixed(3)}`,
              `Y: ${y.toFixed(3)}`
            ];
          }
        }
      }
    },
    interaction: {
      mode: 'nearest',
      intersect: false
    }
  }
});
```

For a fixed readout panel instead of tooltip:

```javascript
canvas.addEventListener('mousemove', (e) => {
  const rect = canvas.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;

  // Get data point at position
  const points = chart.getElementsAtEventForMode(e, 'nearest', { intersect: false }, false);

  if (points.length > 0) {
    const point = points[0];
    const dataX = chart.data.labels[point.index];
    const dataY = chart.data.datasets[point.datasetIndex].data[point.index];

    document.getElementById('readout-x').textContent = dataX.toFixed(3);
    document.getElementById('readout-y').textContent = dataY.toFixed(3);
  }
});
```

---

## 4. Auto-Scaling Axes

### Grace option (padding)

```javascript
options: {
  scales: {
    y: {
      grace: '10%'  // 10% padding above/below data range
    }
  }
}
```

### Suggested bounds

```javascript
options: {
  scales: {
    y: {
      suggestedMin: 0,
      suggestedMax: 100
      // Will expand beyond these if data exceeds
    }
  }
}
```

### Dynamic recalculation

```javascript
function autoScale(chart) {
  const data = chart.data.datasets[0].data;
  const min = Math.min(...data);
  const max = Math.max(...data);
  const padding = (max - min) * 0.1;

  chart.options.scales.y.min = min - padding;
  chart.options.scales.y.max = max + padding;
  chart.update('none');
}
```

---

## 5. Logarithmic Scale

Built-in support:

```javascript
options: {
  scales: {
    y: {
      type: 'logarithmic',
      min: 0.1,  // Must be > 0 for log scale
      ticks: {
        callback: (value) => value.toLocaleString()
      }
    }
  }
}
```

Toggle between linear and log:

```javascript
function toggleLogScale(chart) {
  const currentType = chart.options.scales.y.type;
  chart.options.scales.y.type = currentType === 'logarithmic' ? 'linear' : 'logarithmic';
  chart.update();
}
```

---

## 6. Zoom Controls

### Button-based zoom

```javascript
function zoomIn(chart) {
  const yAxis = chart.scales.y;
  const range = yAxis.max - yAxis.min;
  const center = (yAxis.max + yAxis.min) / 2;
  const newRange = range * 0.8;  // 20% zoom in

  chart.options.scales.y.min = center - newRange / 2;
  chart.options.scales.y.max = center + newRange / 2;
  chart.update('none');
}

function zoomOut(chart) {
  const yAxis = chart.scales.y;
  const range = yAxis.max - yAxis.min;
  const center = (yAxis.max + yAxis.min) / 2;
  const newRange = range * 1.25;  // 25% zoom out

  chart.options.scales.y.min = center - newRange / 2;
  chart.options.scales.y.max = center + newRange / 2;
  chart.update('none');
}

function resetZoom(chart) {
  chart.options.scales.y.min = undefined;
  chart.options.scales.y.max = undefined;
  chart.update();
}
```

### Keyboard shortcuts

```javascript
document.addEventListener('keydown', (e) => {
  if (document.activeElement === chartCanvas) {
    if (e.key === '+' || e.key === '=') zoomIn(chart);
    if (e.key === '-') zoomOut(chart);
    if (e.key === '0') resetZoom(chart);
  }
});
```

---

## 7. Performance Best Practices

### Disable animations for real-time

```javascript
options: {
  animation: false,
  // Or per-update:
  // animation: { duration: 0 }
}
```

### Use 'quiet' updates

```javascript
chart.update('quiet');  // Skip expensive redraws
```

### Data decimation for large datasets

```javascript
options: {
  plugins: {
    decimation: {
      enabled: true,
      algorithm: 'lttb',  // Largest-Triangle-Three-Buckets
      samples: 100
    }
  }
}
```

### Circular buffer for memory

```javascript
class CircularBuffer {
  constructor(size) {
    this.size = size;
    this.data = new Array(size);
    this.head = 0;
    this.count = 0;
  }

  push(value) {
    this.data[this.head] = value;
    this.head = (this.head + 1) % this.size;
    if (this.count < this.size) this.count++;
  }

  toArray() {
    if (this.count < this.size) {
      return this.data.slice(0, this.count);
    }
    return [...this.data.slice(this.head), ...this.data.slice(0, this.head)];
  }
}
```

---

## References

- [chartjs-plugin-crosshair](https://www.npmjs.com/package/chartjs-plugin-crosshair)
- [chartjs-plugin-annotation](https://www.chartjs.org/chartjs-plugin-annotation/latest/)
- [chartjs-plugin-zoom](https://www.chartjs.org/chartjs-plugin-zoom/latest/)
- [Chart.js Performance](https://www.chartjs.org/docs/latest/general/performance.html)
- [Chart.js Logarithmic Axis](https://www.chartjs.org/docs/latest/axes/cartesian/logarithmic.html)

---

*This research informs fc_tool's enhanced plotter implementation.*
