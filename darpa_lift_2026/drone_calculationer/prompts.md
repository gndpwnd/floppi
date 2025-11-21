python main.py
```

## ✨ Key Features

- **13 Modular Tabs** - Each subsystem separated
- **Real-Time Updates** - Change any input, everything recalculates
- **LaTeX Equations** - Beautiful math rendering with MathJax
- **Pass/Fail Checks** - Automatic validation
- **Global State** - Data flows between all tabs seamlessly

## 📊 What Each Tab Does

1. **Constants & Safety** - Global parameters, safety factors
2. **Propeller** - Geometry, coefficients, efficiency
3. **Motors** - KV, torque constant, motor count
4. **Battery** - Voltage, capacity, energy calculations
5. **Airframe** - Structural masses, weight budget
6. **Fixed Wing** - Wing parameters for hybrid VTOL
7. **Mission Profile** - Payload, distances, speeds
8. **Weather** - Air density, wind corrections
9. **Hover Performance** - RPM, power, current in hover
10. **Cruise Performance** - Wing lift, L/D ratio, cruise power
11. **Turn Analysis** - Maneuver energy costs
12. **Energy Budget** - Total mission energy vs battery
13. **Performance Summary** - Dashboard with overall status

## 🔄 How It Works
```
Input Tab (e.g., Propeller) 
  → Updates global state
    → Triggers calculation in dependent tabs
      → Updates all outputs in real-time
        → Summary tab shows final verdict



sudo apt update
sudo apt install python3.11 python3.11-venv python3.11-distutils python3.11-dev

python3.11 -m venv venv

pip install --upgrade pip setuptools wheel
pip install PyQt5 PyQtWebEngine numpy matplotlib dataclasses-json







I would like it to be in table format so taht there is not jsut 2 columns with a short and then a very wide row, if there is overlap in numbers then just hide it in the text boxes. basically i would like to remove the tab system and just have one page. does this make sense? i want to have a grid-like system for everything, the label on top, the value just under the label, then in another cell will be another label and value. I want to then show the equations being used on a grid system as well so that i don't have to scroll through them. please make an id system for an equation to be identified to an input variable, such as equation #1 and then next to the input variable lable, say that it is used by equation #1 and so on. does this make sense?

then same thing for calculated results, have a grid system for display.

the grid system should have a minimum cell width and height, and then be able to dynamically update cell positions and grid sizing based on the window size of the qt5 gui. does this make sense? so at full screen, a griw rwo will have lots of values, but making the gui more narrow will decrease the number of cells in a row, auto fit each cell so that there are no breaks in equations or values or labels between lines please...


i want every tab to have this grid layout, i don't want to have a single page layout. i want each section "inputs, equations, and calculated results" to have a grid layout for every tab...