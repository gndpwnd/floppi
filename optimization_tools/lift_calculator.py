import sys
import math
import numpy as np
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QGridLayout, QLabel, QLineEdit, 
                             QSlider, QPushButton, QTabWidget, QGroupBox,
                             QCheckBox, QComboBox, QTextEdit, QScrollArea,
                             QSpinBox, QDoubleSpinBox)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QFont, QPalette, QColor
import matplotlib.pyplot as plt
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

class DroneCalculator:
    def __init__(self):
        # Physical constants
        self.air_density = 1.225  # kg/m³ at sea level
        self.gravity = 9.81  # m/s²
        
    def calculate_propeller_thrust(self, diameter, pitch, rpm, efficiency=0.8):
        """Calculate theoretical thrust from propeller specifications"""
        # Convert diameter from inches to meters
        diameter_m = diameter * 0.0254
        pitch_m = pitch * 0.0254
        
        # Theoretical advance ratio and thrust coefficient
        advance_ratio = (rpm * pitch_m / 60) / (rpm * diameter_m / 60) if rpm > 0 else 0
        
        # Simplified thrust calculation using momentum theory
        disk_area = math.pi * (diameter_m / 2) ** 2
        tip_speed = (rpm / 60) * math.pi * diameter_m
        
        # Thrust coefficient estimation
        thrust_coeff = 0.1 * (1 - advance_ratio) * efficiency
        thrust_coeff = max(0, thrust_coeff)
        
        # Thrust = Ct * ρ * A * V²
        thrust_newtons = thrust_coeff * self.air_density * disk_area * (tip_speed ** 2)
        thrust_grams = thrust_newtons * 101.97  # Convert N to grams-force
        
        return max(0, thrust_grams)
    
    def calculate_motor_rpm(self, voltage, kv, load_current=0):
        """Calculate motor RPM based on voltage and KV rating"""
        # Simplified motor model
        back_emf_loss = load_current * 0.1  # Simplified resistance loss
        effective_voltage = max(0, voltage - back_emf_loss)
        rpm = kv * effective_voltage
        return rpm
    
    def calculate_power_consumption(self, thrust_grams, motor_efficiency=0.85):
        """Estimate power consumption based on thrust"""
        thrust_newtons = thrust_grams / 101.97
        # Power ≈ (Thrust^1.5) / (efficiency * √(2ρA))
        # Simplified model
        power_watts = (thrust_newtons ** 1.5) / (motor_efficiency * 10)
        return power_watts
    
    def calculate_flight_time(self, battery_capacity_mah, total_power_watts, voltage):
        """Calculate theoretical flight time"""
        if total_power_watts <= 0:
            return 0
        current_amps = total_power_watts / voltage
        flight_time_hours = (battery_capacity_mah / 1000) / current_amps
        return flight_time_hours * 60  # Convert to minutes

class DroneCalculatorGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.calculator = DroneCalculator()
        self.init_ui()
        
    def init_ui(self):
        self.setWindowTitle("Advanced Drone Lift Calculator")
        self.setGeometry(100, 100, 1200, 800)
        
        # Set dark theme
        self.setStyleSheet("""
            QMainWindow { background-color: #2b2b2b; }
            QWidget { background-color: #2b2b2b; color: #ffffff; }
            QGroupBox { 
                border: 2px solid #555555;
                border-radius: 5px;
                margin-top: 10px;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
            }
            QPushButton {
                background-color: #0078d4;
                border: none;
                padding: 8px;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #106ebe; }
            QPushButton:pressed { background-color: #005a9e; }
            QLineEdit, QDoubleSpinBox, QSpinBox {
                background-color: #404040;
                border: 1px solid #555555;
                padding: 5px;
                border-radius: 3px;
            }
            QSlider::groove:horizontal {
                border: 1px solid #555555;
                height: 8px;
                background: #404040;
                border-radius: 4px;
            }
            QSlider::handle:horizontal {
                background: #0078d4;
                border: 1px solid #555555;
                width: 18px;
                margin: -2px 0;
                border-radius: 9px;
            }
            QTabWidget::pane {
                border: 1px solid #555555;
                background-color: #2b2b2b;
            }
            QTabBar::tab {
                background-color: #404040;
                padding: 8px 16px;
                margin-right: 2px;
            }
            QTabBar::tab:selected {
                background-color: #0078d4;
            }
        """)
        
        # Create central widget and main layout
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        # Create tab widget
        self.tab_widget = QTabWidget()
        
        # Create tabs
        self.create_custom_mode_tab()
        self.create_optimize_mode_tab()
        self.create_results_tab()
        
        # Main layout
        main_layout = QVBoxLayout()
        main_layout.addWidget(self.tab_widget)
        central_widget.setLayout(main_layout)
        
        # Initialize values
        self.update_calculations()
    
    def create_custom_mode_tab(self):
        """Create the custom mode tab with all input parameters"""
        custom_tab = QWidget()
        scroll = QScrollArea()
        scroll_widget = QWidget()
        layout = QVBoxLayout()
        
        # Propeller specifications
        prop_group = QGroupBox("Propeller Specifications")
        prop_layout = QGridLayout()
        
        self.prop_diameter = self.create_input_row(prop_layout, 0, "Diameter (inches):", 10.0, 1.0, 20.0)
        self.prop_pitch = self.create_input_row(prop_layout, 1, "Pitch (inches):", 4.5, 1.0, 15.0)
        self.prop_blades = self.create_spin_row(prop_layout, 2, "Number of Blades:", 2, 2, 8)
        self.prop_efficiency = self.create_input_row(prop_layout, 3, "Efficiency (%):", 80.0, 50.0, 95.0)
        self.prop_angle = self.create_input_row(prop_layout, 4, "Blade Angle (degrees):", 15.0, 5.0, 45.0)
        
        prop_group.setLayout(prop_layout)
        layout.addWidget(prop_group)
        
        # Motor specifications
        motor_group = QGroupBox("Motor Specifications")
        motor_layout = QGridLayout()
        
        self.motor_kv = self.create_input_row(motor_layout, 0, "KV Rating (RPM/V):", 1000.0, 100.0, 5000.0)
        self.motor_power = self.create_input_row(motor_layout, 1, "Max Power (W):", 200.0, 10.0, 1000.0)
        self.motor_efficiency = self.create_input_row(motor_layout, 2, "Efficiency (%):", 85.0, 70.0, 95.0)
        self.motor_weight = self.create_input_row(motor_layout, 3, "Weight (g):", 50.0, 10.0, 500.0)
        self.num_motors = self.create_spin_row(motor_layout, 4, "Number of Motors:", 4, 1, 8)
        
        motor_group.setLayout(motor_layout)
        layout.addWidget(motor_group)
        
        # Battery/Power specifications
        battery_group = QGroupBox("Battery & Power System")
        battery_layout = QGridLayout()
        
        self.battery_voltage = self.create_input_row(battery_layout, 0, "Voltage (V):", 11.1, 3.7, 48.0)
        self.battery_capacity = self.create_input_row(battery_layout, 1, "Capacity (mAh):", 5000.0, 500.0, 20000.0)
        self.battery_weight = self.create_input_row(battery_layout, 2, "Battery Weight (g):", 300.0, 50.0, 2000.0)
        self.esc_efficiency = self.create_input_row(battery_layout, 3, "ESC Efficiency (%):", 95.0, 80.0, 98.0)
        self.esc_weight = self.create_input_row(battery_layout, 4, "ESC Weight (g):", 25.0, 5.0, 100.0)
        
        battery_group.setLayout(battery_layout)
        layout.addWidget(battery_group)
        
        # Frame specifications
        frame_group = QGroupBox("Frame & Structure")
        frame_layout = QGridLayout()
        
        self.frame_weight = self.create_input_row(frame_layout, 0, "Frame Weight (g):", 200.0, 50.0, 1000.0)
        self.frame_drag_coeff = self.create_input_row(frame_layout, 1, "Drag Coefficient:", 0.5, 0.1, 2.0)
        self.frame_area = self.create_input_row(frame_layout, 2, "Frontal Area (cm²):", 100.0, 20.0, 500.0)
        
        frame_group.setLayout(frame_layout)
        layout.addWidget(frame_group)
        
        # Payload specifications
        payload_group = QGroupBox("Payload & Electronics")
        payload_layout = QGridLayout()
        
        self.flight_controller_weight = self.create_input_row(payload_layout, 0, "Flight Controller (g):", 30.0, 10.0, 200.0)
        self.camera_weight = self.create_input_row(payload_layout, 1, "Camera/Gimbal (g):", 100.0, 0.0, 1000.0)
        self.additional_payload = self.create_input_row(payload_layout, 2, "Additional Payload (g):", 0.0, 0.0, 5000.0)
        self.wiring_weight = self.create_input_row(payload_layout, 3, "Wiring/Misc (g):", 50.0, 10.0, 200.0)
        
        payload_group.setLayout(payload_layout)
        layout.addWidget(payload_group)
        
        # Environmental conditions
        env_group = QGroupBox("Environmental Conditions")
        env_layout = QGridLayout()
        
        self.air_density = self.create_input_row(env_layout, 0, "Air Density (kg/m³):", 1.225, 0.8, 1.3)
        self.wind_speed = self.create_input_row(env_layout, 1, "Wind Speed (m/s):", 0.0, 0.0, 20.0)
        self.temperature = self.create_input_row(env_layout, 2, "Temperature (°C):", 20.0, -20.0, 50.0)
        
        env_group.setLayout(env_layout)
        layout.addWidget(env_group)
        
        # Calculate button
        calc_button = QPushButton("Calculate Performance")
        calc_button.clicked.connect(self.update_calculations)
        layout.addWidget(calc_button)
        
        scroll_widget.setLayout(layout)
        scroll.setWidget(scroll_widget)
        scroll.setWidgetResizable(True)
        
        self.tab_widget.addTab(scroll, "Custom Mode")
        
    def create_optimize_mode_tab(self):
        """Create the optimize mode tab with checkboxes for optimization"""
        optimize_tab = QWidget()
        layout = QVBoxLayout()
        
        # Optimization parameters
        opt_group = QGroupBox("Optimization Parameters")
        opt_layout = QGridLayout()
        
        self.opt_prop_diameter = QCheckBox("Optimize Propeller Diameter")
        self.opt_motor_kv = QCheckBox("Optimize Motor KV")
        self.opt_battery_capacity = QCheckBox("Optimize Battery Capacity")
        self.opt_num_motors = QCheckBox("Optimize Number of Motors")
        
        opt_layout.addWidget(self.opt_prop_diameter, 0, 0)
        opt_layout.addWidget(self.opt_motor_kv, 0, 1)
        opt_layout.addWidget(self.opt_battery_capacity, 1, 0)
        opt_layout.addWidget(self.opt_num_motors, 1, 1)
        
        opt_group.setLayout(opt_layout)
        layout.addWidget(opt_group)
        
        # Optimization constraints
        constraint_group = QGroupBox("Constraints")
        constraint_layout = QGridLayout()
        
        self.max_weight = self.create_input_row(constraint_layout, 0, "Max Total Weight (g):", 2000.0, 500.0, 10000.0)
        self.min_flight_time = self.create_input_row(constraint_layout, 1, "Min Flight Time (min):", 10.0, 1.0, 120.0)
        self.max_cost = self.create_input_row(constraint_layout, 2, "Max Cost Budget ($):", 500.0, 100.0, 5000.0)
        
        constraint_group.setLayout(constraint_layout)
        layout.addWidget(constraint_group)
        
        # Optimize button
        optimize_button = QPushButton("Run Optimization")
        optimize_button.clicked.connect(self.run_optimization)
        layout.addWidget(optimize_button)
        
        # Results area
        self.optimize_results = QTextEdit()
        self.optimize_results.setMaximumHeight(300)
        layout.addWidget(self.optimize_results)
        
        optimize_tab.setLayout(layout)
        self.tab_widget.addTab(optimize_tab, "Optimize Mode")
    
    def create_results_tab(self):
        """Create the results tab with calculations and graphs"""
        results_tab = QWidget()
        layout = QVBoxLayout()
        
        # Results display
        results_group = QGroupBox("Performance Results")
        results_layout = QGridLayout()
        
        self.result_labels = {}
        result_items = [
            "Total Weight", "Total Thrust", "Thrust-to-Weight Ratio",
            "Flight Time", "Power Consumption", "Max Payload Capacity"
        ]
        
        for i, item in enumerate(result_items):
            label = QLabel(f"{item}:")
            value_label = QLabel("0.0")
            value_label.setStyleSheet("font-weight: bold; color: #00ff00;")
            results_layout.addWidget(label, i, 0)
            results_layout.addWidget(value_label, i, 1)
            self.result_labels[item] = value_label
        
        results_group.setLayout(results_layout)
        layout.addWidget(results_group)
        
        # Performance graph
        self.figure = Figure(figsize=(10, 6), facecolor='#2b2b2b')
        self.canvas = FigureCanvas(self.figure)
        layout.addWidget(self.canvas)
        
        # Analysis text
        self.analysis_text = QTextEdit()
        self.analysis_text.setMaximumHeight(150)
        layout.addWidget(self.analysis_text)
        
        results_tab.setLayout(layout)
        self.tab_widget.addTab(results_tab, "Results & Analysis")
    
    def create_input_row(self, layout, row, label_text, default_value, min_val, max_val):
        """Helper to create an input row with label, spinbox, and slider"""
        label = QLabel(label_text)
        spinbox = QDoubleSpinBox()
        spinbox.setRange(min_val, max_val)
        spinbox.setValue(default_value)
        spinbox.setDecimals(1)
        spinbox.setSingleStep(0.1)
        
        slider = QSlider(Qt.Horizontal)
        slider.setRange(int(min_val * 10), int(max_val * 10))
        slider.setValue(int(default_value * 10))
        
        # Connect spinbox and slider
        spinbox.valueChanged.connect(lambda v: slider.setValue(int(v * 10)))
        slider.valueChanged.connect(lambda v: spinbox.setValue(v / 10.0))
        spinbox.valueChanged.connect(self.update_calculations)
        
        layout.addWidget(label, row, 0)
        layout.addWidget(spinbox, row, 1)
        layout.addWidget(slider, row, 2)
        
        return spinbox
    
    def create_spin_row(self, layout, row, label_text, default_value, min_val, max_val):
        """Helper to create an integer spin row"""
        label = QLabel(label_text)
        spinbox = QSpinBox()
        spinbox.setRange(min_val, max_val)
        spinbox.setValue(default_value)
        
        spinbox.valueChanged.connect(self.update_calculations)
        
        layout.addWidget(label, row, 0)
        layout.addWidget(spinbox, row, 1)
        
        return spinbox
    
    def update_calculations(self):
        """Update all calculations and display results"""
        try:
            # Get all input values
            prop_diameter = self.prop_diameter.value()
            prop_pitch = self.prop_pitch.value()
            motor_kv = self.motor_kv.value()
            battery_voltage = self.battery_voltage.value()
            num_motors = self.num_motors.value()
            
            # Calculate motor RPM
            rpm = self.calculator.calculate_motor_rpm(battery_voltage, motor_kv)
            
            # Calculate thrust per motor
            thrust_per_motor = self.calculator.calculate_propeller_thrust(
                prop_diameter, prop_pitch, rpm, self.prop_efficiency.value() / 100.0
            )
            
            # Total thrust
            total_thrust = thrust_per_motor * num_motors
            
            # Calculate total weight
            total_weight = (
                self.motor_weight.value() * num_motors +
                self.battery_weight.value() +
                self.esc_weight.value() * num_motors +
                self.frame_weight.value() +
                self.flight_controller_weight.value() +
                self.camera_weight.value() +
                self.additional_payload.value() +
                self.wiring_weight.value()
            )
            
            # Thrust-to-weight ratio
            twr = total_thrust / total_weight if total_weight > 0 else 0
            
            # Power consumption
            power_per_motor = self.calculator.calculate_power_consumption(
                thrust_per_motor, self.motor_efficiency.value() / 100.0
            )
            total_power = power_per_motor * num_motors
            
            # Flight time
            flight_time = self.calculator.calculate_flight_time(
                self.battery_capacity.value(), total_power, battery_voltage
            )
            
            # Max payload capacity (when TWR = 2.0 for good performance)
            max_payload = max(0, (total_thrust / 2.0) - (total_weight - self.additional_payload.value()))
            
            # Update result labels
            self.result_labels["Total Weight"].setText(f"{total_weight:.1f} g")
            self.result_labels["Total Thrust"].setText(f"{total_thrust:.1f} g")
            self.result_labels["Thrust-to-Weight Ratio"].setText(f"{twr:.2f}")
            self.result_labels["Flight Time"].setText(f"{flight_time:.1f} min")
            self.result_labels["Power Consumption"].setText(f"{total_power:.1f} W")
            self.result_labels["Max Payload Capacity"].setText(f"{max_payload:.1f} g")
            
            # Color code TWR
            if twr < 1.0:
                self.result_labels["Thrust-to-Weight Ratio"].setStyleSheet("font-weight: bold; color: #ff0000;")
            elif twr < 1.5:
                self.result_labels["Thrust-to-Weight Ratio"].setStyleSheet("font-weight: bold; color: #ffff00;")
            else:
                self.result_labels["Thrust-to-Weight Ratio"].setStyleSheet("font-weight: bold; color: #00ff00;")
            
            # Update analysis
            self.update_analysis(twr, flight_time, total_power, total_weight)
            
            # Update graph
            self.update_performance_graph()
            
        except Exception as e:
            print(f"Calculation error: {e}")
    
    def update_analysis(self, twr, flight_time, power, weight):
        """Update the analysis text with recommendations"""
        analysis = []
        
        if twr < 1.0:
            analysis.append("❌ CRITICAL: Thrust-to-weight ratio below 1.0 - drone cannot fly!")
            analysis.append("Recommendations: Increase propeller size, motor power, or reduce weight.")
        elif twr < 1.5:
            analysis.append("⚠️ WARNING: Low thrust-to-weight ratio - limited maneuverability.")
            analysis.append("Recommendations: Consider larger propellers or more powerful motors.")
        else:
            analysis.append("✅ GOOD: Sufficient thrust-to-weight ratio for stable flight.")
        
        if flight_time < 5:
            analysis.append("❌ Very short flight time - consider larger battery or more efficient setup.")
        elif flight_time < 15:
            analysis.append("⚠️ Moderate flight time - acceptable for short missions.")
        else:
            analysis.append("✅ Good flight time for extended operations.")
        
        if power > 500:
            analysis.append("⚠️ High power consumption - may cause overheating.")
        
        analysis.append(f"\nTotal system weight: {weight:.1f}g")
        analysis.append(f"Power loading: {power/weight:.2f} W/g")
        
        self.analysis_text.setText("\n".join(analysis))
    
    def update_performance_graph(self):
        """Update the performance graph"""
        self.figure.clear()
        
        # Create subplots
        ax1 = self.figure.add_subplot(221)
        ax2 = self.figure.add_subplot(222)
        ax3 = self.figure.add_subplot(223)
        ax4 = self.figure.add_subplot(224)
        
        # Set dark theme for plots
        for ax in [ax1, ax2, ax3, ax4]:
            ax.set_facecolor('#404040')
            ax.tick_params(colors='white')
            ax.xaxis.label.set_color('white')
            ax.yaxis.label.set_color('white')
            ax.title.set_color('white')
        
        # Plot 1: Thrust vs RPM
        rpms = np.linspace(1000, 8000, 50)
        thrusts = [self.calculator.calculate_propeller_thrust(
            self.prop_diameter.value(), self.prop_pitch.value(), rpm
        ) for rpm in rpms]
        
        ax1.plot(rpms, thrusts, 'cyan', linewidth=2)
        ax1.set_xlabel('RPM')
        ax1.set_ylabel('Thrust (g)')
        ax1.set_title('Thrust vs RPM')
        ax1.grid(True, alpha=0.3)
        
        # Plot 2: Power vs Thrust
        thrust_range = np.linspace(100, 2000, 50)
        powers = [self.calculator.calculate_power_consumption(t) for t in thrust_range]
        
        ax2.plot(thrust_range, powers, 'orange', linewidth=2)
        ax2.set_xlabel('Thrust (g)')
        ax2.set_ylabel('Power (W)')
        ax2.set_title('Power vs Thrust')
        ax2.grid(True, alpha=0.3)
        
        # Plot 3: Flight Time vs Battery Capacity
        capacities = np.linspace(1000, 10000, 50)
        current_power = self.calculator.calculate_power_consumption(
            self.calculator.calculate_propeller_thrust(
                self.prop_diameter.value(), self.prop_pitch.value(),
                self.calculator.calculate_motor_rpm(self.battery_voltage.value(), self.motor_kv.value())
            ) * self.num_motors.value()
        )
        
        flight_times = [self.calculator.calculate_flight_time(
            cap, current_power, self.battery_voltage.value()
        ) for cap in capacities]
        
        ax3.plot(capacities, flight_times, 'lime', linewidth=2)
        ax3.set_xlabel('Battery Capacity (mAh)')
        ax3.set_ylabel('Flight Time (min)')
        ax3.set_title('Flight Time vs Battery Capacity')
        ax3.grid(True, alpha=0.3)
        
        # Plot 4: TWR vs Propeller Diameter
        diameters = np.linspace(6, 15, 50)
        twrs = []
        total_weight = (
            self.motor_weight.value() * self.num_motors.value() +
            self.battery_weight.value() + self.frame_weight.value() +
            self.flight_controller_weight.value() + self.camera_weight.value() +
            self.additional_payload.value() + self.wiring_weight.value()
        )
        
        for diameter in diameters:
            rpm = self.calculator.calculate_motor_rpm(self.battery_voltage.value(), self.motor_kv.value())
            thrust = self.calculator.calculate_propeller_thrust(
                diameter, self.prop_pitch.value(), rpm
            ) * self.num_motors.value()
            twr = thrust / total_weight if total_weight > 0 else 0
            twrs.append(twr)
        
        ax4.plot(diameters, twrs, 'magenta', linewidth=2)
        ax4.axhline(y=1.0, color='red', linestyle='--', alpha=0.7, label='Min TWR')
        ax4.axhline(y=2.0, color='green', linestyle='--', alpha=0.7, label='Good TWR')
        ax4.set_xlabel('Propeller Diameter (inches)')
        ax4.set_ylabel('Thrust-to-Weight Ratio')
        ax4.set_title('TWR vs Propeller Diameter')
        ax4.grid(True, alpha=0.3)
        ax4.legend()
        
        self.figure.tight_layout()
        self.canvas.draw()
    
    def run_optimization(self):
        """Run optimization based on selected parameters"""
        results = ["=== OPTIMIZATION RESULTS ===\n"]
        
        # Check which parameters to optimize
        optimize_params = []
        if self.opt_prop_diameter.isChecked():
            optimize_params.append("Propeller Diameter")
        if self.opt_motor_kv.isChecked():
            optimize_params.append("Motor KV")
        if self.opt_battery_capacity.isChecked():
            optimize_params.append("Battery Capacity")
        if self.opt_num_motors.isChecked():
            optimize_params.append("Number of Motors")
        
        if not optimize_params:
            results.append("❌ No parameters selected for optimization!")
            self.optimize_results.setText("\n".join(results))
            return
        
        results.append(f"Optimizing: {', '.join(optimize_params)}\n")
        
        # Simple optimization - test different combinations
        best_config = None
        best_score = 0
        
        # Test ranges
        prop_diameters = [8, 10, 12, 14] if "Propeller Diameter" in optimize_params else [self.prop_diameter.value()]
        motor_kvs = [800, 1000, 1200, 1500] if "Motor KV" in optimize_params else [self.motor_kv.value()]
        battery_caps = [3000, 5000, 8000, 10000] if "Battery Capacity" in optimize_params else [self.battery_capacity.value()]
        motor_counts = [3, 4, 6, 8] if "Number of Motors" in optimize_params else [self.num_motors.value()]
        
        for prop_d in prop_diameters:
            for motor_kv in motor_kvs:
                for battery_cap in battery_caps:
                    for motor_count in motor_counts:
                        # Calculate performance for this configuration
                        rpm = self.calculator.calculate_motor_rpm(self.battery_voltage.value(), motor_kv)
                        thrust_per_motor = self.calculator.calculate_propeller_thrust(prop_d, self.prop_pitch.value(), rpm)
                        total_thrust = thrust_per_motor * motor_count
                        
                        total_weight = (
                            self.motor_weight.value() * motor_count +
                            self.battery_weight.value() +
                            self.frame_weight.value() +
                            self.flight_controller_weight.value() +
                            self.camera_weight.value() +
                            self.additional_payload.value() +
                            self.wiring_weight.value()
                        )
                        
                        twr = total_thrust / total_weight if total_weight > 0 else 0
                        
                        power = self.calculator.calculate_power_consumption(thrust_per_motor) * motor_count
                        flight_time = self.calculator.calculate_flight_time(
                            battery_cap, power, self.battery_voltage.value()
                        )
                        
                        # Check constraints
                        if total_weight > self.max_weight.value():
                            continue
                        if flight_time < self.min_flight_time.value():
                            continue
                        
                        # Calculate optimization score (weighted combination of factors)
                        score = (twr * 30) + (flight_time * 2) - (total_weight * 0.01) - (power * 0.1)
                        
                        if score > best_score and twr >= 1.0:
                            best_score = score
                            best_config = {
                                'prop_diameter': prop_d,
                                'motor_kv': motor_kv,
                                'battery_capacity': battery_cap,
                                'motor_count': motor_count,
                                'twr': twr,
                                'flight_time': flight_time,
                                'total_weight': total_weight,
                                'power': power,
                                'score': score
                            }
        
        if best_config:
            results.append("✅ OPTIMAL CONFIGURATION FOUND:")
            results.append(f"Propeller Diameter: {best_config['prop_diameter']} inches")
            results.append(f"Motor KV: {best_config['motor_kv']} RPM/V")
            results.append(f"Battery Capacity: {best_config['battery_capacity']} mAh")
            results.append(f"Number of Motors: {best_config['motor_count']}")
            results.append("")
            results.append("PERFORMANCE METRICS:")
            results.append(f"Thrust-to-Weight Ratio: {best_config['twr']:.2f}")
            results.append(f"Flight Time: {best_config['flight_time']:.1f} minutes")
            results.append(f"Total Weight: {best_config['total_weight']:.1f} g")
            results.append(f"Power Consumption: {best_config['power']:.1f} W")
            results.append(f"Optimization Score: {best_config['score']:.1f}")
        else:
            results.append("❌ No valid configuration found within constraints!")
            results.append("Try relaxing constraints or selecting different optimization parameters.")
        
        self.optimize_results.setText("\n".join(results))

def main():
    app = QApplication(sys.argv)
    
    # Set application style
    app.setStyle('Fusion')
    
    # Create and show the main window
    window = DroneCalculatorGUI()
    window.show()
    
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()