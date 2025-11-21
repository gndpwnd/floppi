"""
Enhanced Base Tab with Dynamic Grid Layout
Complete implementation with all original features
"""

from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QGridLayout,
                              QLabel, QLineEdit, QPushButton, QGroupBox, 
                              QScrollArea, QCheckBox, QComboBox, QFrame)
from PyQt5.QtCore import Qt, pyqtSignal, QTimer
from PyQt5.QtGui import QFont
import math


class GridContainer(QWidget):
    """Responsive grid container that reflows based on window size"""
    
    def __init__(self, min_cell_width=200, min_cell_height=60):
        super().__init__()
        self.min_cell_width = min_cell_width
        self.min_cell_height = min_cell_height
        self.cells = []  # List of (widget, priority) tuples
        self.layout = QGridLayout()
        self.layout.setSpacing(10)
        self.layout.setContentsMargins(10, 10, 10, 10)
        self.setLayout(self.layout)
        self.current_cols = 0
        self.reflow_timer = QTimer()
        self.reflow_timer.setSingleShot(True)
        self.reflow_timer.timeout.connect(self._do_reflow)
        self.last_width = 0
        
    def add_cell(self, widget, priority=0):
        """Add a cell to the grid. Higher priority = earlier in layout"""
        self.cells.append((widget, priority))
        self.schedule_reflow()
        
    def clear_cells(self):
        """Remove all cells"""
        for cell, _ in self.cells:
            self.layout.removeWidget(cell)
            cell.setParent(None)
        self.cells.clear()
        
    def schedule_reflow(self):
        """Schedule a reflow (debounced)"""
        self.reflow_timer.stop()
        self.reflow_timer.start(200)  # 200ms debounce
        
    def _do_reflow(self):
        """Actually perform the reflow"""
        if not self.cells:
            return
            
        # Calculate columns based on width
        available_width = self.width() if self.width() > 0 else 800
        cols = max(1, available_width // self.min_cell_width)
        
        # Only reflow if column count actually changed
        if cols == self.current_cols:
            return
            
        self.current_cols = cols
        
        # Sort by priority
        sorted_cells = sorted(self.cells, key=lambda x: x[1], reverse=True)
        
        # Clear current layout WITHOUT removing widgets from parent
        for i in reversed(range(self.layout.count())): 
            item = self.layout.itemAt(i)
            if item:
                self.layout.removeItem(item)
        
        # Add cells in grid
        for idx, (cell, _) in enumerate(sorted_cells):
            row = idx // cols
            col = idx % cols
            self.layout.addWidget(cell, row, col)
            cell.setMinimumHeight(self.min_cell_height)
            
    def resizeEvent(self, event):
        """Handle resize to reflow grid"""
        super().resizeEvent(event)
        
        # Only schedule reflow if width changed significantly
        new_width = event.size().width()
        if abs(new_width - self.last_width) > 50:  # 50px threshold
            self.last_width = new_width
            self.schedule_reflow()


class ParameterCell(QFrame):
    """Individual parameter cell with label and input"""
    
    value_changed = pyqtSignal()
    
    def __init__(self, label, key, default, unit="", tooltip="", equation_refs=None):
        super().__init__()
        self.key = key
        self.equation_refs = equation_refs or []
        
        self.setFrameStyle(QFrame.Box | QFrame.Plain)
        self.setLineWidth(1)
        
        layout = QVBoxLayout()
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(4)
        
        # Label with equation references
        label_text = f"<b>{label}</b>"
        if self.equation_refs:
            eq_refs = ", ".join([f"Eq {ref}" for ref in self.equation_refs])
            label_text += f"<br><font size='2' color='#666'>[{eq_refs}]</font>"
        
        self.label = QLabel(label_text)
        self.label.setWordWrap(True)
        layout.addWidget(self.label)
        
        # Input field
        self.input = QLineEdit(str(default))
        self.input.setToolTip(tooltip)
        self.input.textChanged.connect(self.value_changed.emit)
        layout.addWidget(self.input)
        
        # Unit label
        if unit:
            unit_label = QLabel(f"<font size='2' color='#666'>{unit}</font>")
            layout.addWidget(unit_label)
        
        self.setLayout(layout)
        
        # Styling
        self.setStyleSheet("""
            ParameterCell {
                background-color: #fafafa;
                border: 1px solid #ddd;
                border-radius: 4px;
            }
            ParameterCell:hover {
                border: 1px solid #4CAF50;
                background-color: #f0f8f0;
            }
        """)
        
    def get_value(self, value_type=float):
        """Get the input value"""
        try:
            return value_type(self.input.text())
        except ValueError:
            return 0.0 if value_type == float else 0


class CheckboxCell(QFrame):
    """Cell for checkbox input"""
    
    value_changed = pyqtSignal()
    
    def __init__(self, label, key, default=False, tooltip="", equation_refs=None):
        super().__init__()
        self.key = key
        self.equation_refs = equation_refs or []
        
        self.setFrameStyle(QFrame.Box | QFrame.Plain)
        self.setLineWidth(1)
        
        layout = QVBoxLayout()
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(4)
        
        # Label with equation references
        label_text = f"<b>{label}</b>"
        if self.equation_refs:
            eq_refs = ", ".join([f"Eq {ref}" for ref in self.equation_refs])
            label_text += f"<br><font size='2' color='#666'>[{eq_refs}]</font>"
        
        self.label = QLabel(label_text)
        self.label.setWordWrap(True)
        layout.addWidget(self.label)
        
        # Checkbox
        self.checkbox = QCheckBox()
        self.checkbox.setChecked(default)
        self.checkbox.setToolTip(tooltip)
        self.checkbox.stateChanged.connect(self.value_changed.emit)
        layout.addWidget(self.checkbox)
        
        self.setLayout(layout)
        
        # Styling
        self.setStyleSheet("""
            CheckboxCell {
                background-color: #fafafa;
                border: 1px solid #ddd;
                border-radius: 4px;
            }
            CheckboxCell:hover {
                border: 1px solid #4CAF50;
                background-color: #f0f8f0;
            }
        """)
        
    def get_value(self):
        """Get the checkbox state"""
        return self.checkbox.isChecked()


class OutputCell(QFrame):
    """Display cell for calculated results"""
    
    def __init__(self, label, key, unit="", decimals=4, equation_refs=None):
        super().__init__()
        self.key = key
        self.decimals = decimals
        self.equation_refs = equation_refs or []
        
        self.setFrameStyle(QFrame.Box | QFrame.Plain)
        self.setLineWidth(1)
        
        layout = QVBoxLayout()
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(4)
        
        # Label with equation references
        label_text = f"<b>{label}</b>"
        if self.equation_refs:
            eq_refs = ", ".join([f"Eq {ref}" for ref in self.equation_refs])
            label_text += f"<br><font size='2' color='#666'>[{eq_refs}]</font>"
        
        self.label = QLabel(label_text)
        self.label.setWordWrap(True)
        layout.addWidget(self.label)
        
        # Value display
        self.value_label = QLabel("—")
        font = QFont()
        font.setPointSize(12)
        font.setBold(True)
        self.value_label.setFont(font)
        self.value_label.setStyleSheet("color: #2c3e50;")
        self.value_label.setWordWrap(True)
        layout.addWidget(self.value_label)
        
        # Unit label
        if unit:
            unit_label = QLabel(f"<font size='2' color='#666'>{unit}</font>")
            layout.addWidget(unit_label)
        
        self.setLayout(layout)
        
        # Styling
        self.setStyleSheet("""
            OutputCell {
                background-color: #e8f5e9;
                border: 1px solid #c8e6c9;
                border-radius: 4px;
            }
        """)
        
    def set_value(self, value, status=""):
        """Set the display value"""
        if isinstance(value, (int, float)):
            text = f"{value:.{self.decimals}f}"
        else:
            text = str(value)
            
        if status:
            text += f" ({status})"
            
        self.value_label.setText(text)


class EquationCell(QFrame):
    """Cell displaying a single equation"""
    
    def __init__(self, eq_id, equation_latex, description=""):
        super().__init__()
        self.eq_id = eq_id
        
        self.setFrameStyle(QFrame.Box | QFrame.Plain)
        self.setLineWidth(1)
        
        layout = QVBoxLayout()
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(4)
        
        # Equation ID
        id_label = QLabel(f"<b>Equation {eq_id}</b>")
        id_label.setStyleSheet("color: #1976d2;")
        layout.addWidget(id_label)
        
        # Equation (simplified text representation)
        eq_label = QLabel(f"<font face='monospace' size='3'>{equation_latex}</font>")
        eq_label.setWordWrap(True)
        layout.addWidget(eq_label)
        
        # Description
        if description:
            desc_label = QLabel(f"<font size='2' color='#666'>{description}</font>")
            desc_label.setWordWrap(True)
            layout.addWidget(desc_label)
        
        self.setLayout(layout)
        
        # Styling
        self.setStyleSheet("""
            EquationCell {
                background-color: #fff3e0;
                border: 1px solid #ffe0b2;
                border-radius: 4px;
            }
        """)


class BaseTab(QWidget):
    """Enhanced base class with grid layout"""
    
    calculation_done = pyqtSignal()
    
    def __init__(self, state, title):
        super().__init__()
        self.state = state
        self.title = title
        self.input_cells = {}
        self.output_cells = {}
        self.equation_map = {}  # Maps equation ID to latex/description
        
        self.init_ui()
        
        # Connect to global state changes
        self.state.state_changed.connect(self.on_external_update)
        
    def init_ui(self):
        """Initialize the UI layout"""
        main_layout = QVBoxLayout()
        main_layout.setSpacing(15)
        
        # Title
        title_label = QLabel(f"<h2>{self.title}</h2>")
        main_layout.addWidget(title_label)
        
        # Scrollable area
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        
        scroll_content = QWidget()
        scroll_layout = QVBoxLayout(scroll_content)
        scroll_layout.setSpacing(15)
        
        # Input Parameters Grid
        inputs_label = QLabel("<h3>Input Parameters</h3>")
        scroll_layout.addWidget(inputs_label)
        
        self.inputs_grid = GridContainer(min_cell_width=220, min_cell_height=90)
        scroll_layout.addWidget(self.inputs_grid)
        
        # Equations Grid
        equations_label = QLabel("<h3>Equations</h3>")
        scroll_layout.addWidget(equations_label)
        
        self.equations_grid = GridContainer(min_cell_width=300, min_cell_height=100)
        scroll_layout.addWidget(self.equations_grid)
        
        # Calculated Results Grid
        outputs_label = QLabel("<h3>Calculated Results</h3>")
        scroll_layout.addWidget(outputs_label)
        
        self.outputs_grid = GridContainer(min_cell_width=220, min_cell_height=90)
        scroll_layout.addWidget(self.outputs_grid)
        
        scroll_layout.addStretch()
        scroll.setWidget(scroll_content)
        main_layout.addWidget(scroll)
        
        self.setLayout(main_layout)
        
        # Build specific tab content
        self.define_equations()
        self.build_inputs()
        self.build_outputs()
        self.populate_equation_grid()
        
        # Initial calculation
        self.calculate()
        
    def add_input_field(self, label, key, default, unit="", tooltip="", equation_refs=None, priority=0):
        """Add an input field to the grid"""
        cell = ParameterCell(label, key, default, unit, tooltip, equation_refs)
        cell.value_changed.connect(lambda: self.on_input_changed(key))
        self.input_cells[key] = cell
        self.inputs_grid.add_cell(cell, priority)
        return cell
        
    def add_checkbox(self, label, key, default=False, tooltip="", equation_refs=None, priority=0):
        """Add a checkbox to the grid"""
        cell = CheckboxCell(label, key, default, tooltip, equation_refs)
        cell.value_changed.connect(lambda: self.on_input_changed(key))
        self.input_cells[key] = cell
        self.inputs_grid.add_cell(cell, priority)
        return cell
        
    def add_output_field(self, label, key, unit="", decimals=4, equation_refs=None, priority=0):
        """Add an output field to the grid"""
        cell = OutputCell(label, key, unit, decimals, equation_refs)
        self.output_cells[key] = cell
        self.outputs_grid.add_cell(cell, priority)
        return cell
        
    def get_input_value(self, key, value_type=float):
        """Get value from input field"""
        if key in self.input_cells:
            cell = self.input_cells[key]
            if isinstance(cell, CheckboxCell):
                return cell.get_value()
            else:
                return cell.get_value(value_type)
        return None
        
    def set_output_value(self, key, value, status=""):
        """Set value in output field"""
        if key in self.output_cells:
            self.output_cells[key].set_value(value, status)
            
    def add_equation(self, eq_id, latex, description=""):
        """Register an equation"""
        self.equation_map[eq_id] = (latex, description)
        
    def populate_equation_grid(self):
        """Add all equations to the grid"""
        for eq_id in sorted(self.equation_map.keys()):
            latex, description = self.equation_map[eq_id]
            cell = EquationCell(eq_id, latex, description)
            self.equations_grid.add_cell(cell)
            
    def on_input_changed(self, key):
        """Handle input change"""
        self.calculate()
        self.calculation_done.emit()
        
    def on_external_update(self):
        """Handle updates from other tabs"""
        self.calculate()
        
    def calculate(self):
        """Override in subclasses to perform calculations"""
        pass
        
    def build_inputs(self):
        """Override in subclasses to build input fields"""
        pass
        
    def build_outputs(self):
        """Override in subclasses to build output fields"""
        pass
        
    def define_equations(self):
        """Override in subclasses to define equations"""
        pass
        
    def get_input_values(self):
        """Get all input values from this tab for serialization"""
        input_values = {}
        for key, cell in self.input_cells.items():
            if isinstance(cell, CheckboxCell):
                input_values[key] = cell.get_value()
            else:
                input_values[key] = cell.input.text()
        return input_values
        
    def set_input_values(self, input_values):
        """Set input values for this tab from serialization"""
        for key, value in input_values.items():
            if key in self.input_cells:
                cell = self.input_cells[key]
                if isinstance(cell, CheckboxCell):
                    cell.checkbox.setChecked(bool(value))
                else:
                    cell.input.setText(str(value))
        # Trigger calculation after setting values
        self.calculate()