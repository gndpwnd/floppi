"""
All tab implementations with grid layout
PyQt5 Version - Complete Update
"""

from base_tab import BaseTab
import math


# ============================================================================
# SHEET 1: CONSTANTS & SAFETY
# ============================================================================

class ConstantsSafetyTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Constants & Safety Factors")
        
    def define_equations(self):
        self.add_equation(1, "ρ = P / (R × T)", "Air density from ideal gas law")
        self.add_equation(2, "V_net = max(0, V_head - V_tail)", "Net wind velocity")
        
    def build_inputs(self):
        self.add_input_field("Safety Factor", "safety_factor", 1.2, "-", 
                           "Applied to thrust, energy, structural loads", priority=10)
        self.add_input_field("Time Limit", "time_limit", 30, "min", priority=9)
        self.add_input_field("Drone Weight Limit", "drone_weight_limit", 24.95, "kg", priority=8)
        self.add_input_field("Minimum Payload", "min_payload", 49.9, "kg", priority=7)
        self.add_input_field("Gravity", "gravity", 9.81, "m/s²", equation_refs=[2], priority=6)
        self.add_input_field("Air Density (Sea Level)", "air_density_sl", 1.225, "kg/m³", 
                           equation_refs=[1], priority=5)
        self.add_input_field("Speed of Sound", "speed_of_sound", 343, "m/s", priority=4)
        self.add_input_field("CF Yield Strength", "cf_yield", 600, "MPa", priority=3)
        self.add_input_field("CF Density", "cf_density", 1600, "kg/m³", priority=2)
        self.add_input_field("Al Yield Strength", "al_yield", 276, "MPa", priority=1)
        self.add_input_field("Al Density", "al_density", 2700, "kg/m³", priority=0)
        
    def build_outputs(self):
        self.add_output_field("Safety Factor Check", "sf_check", "", decimals=0, priority=10)
        
    def calculate(self):
        # Store constants in global state
        self.state.set_value("safety_factor", self.get_input_value("safety_factor"))
        self.state.set_value("gravity", self.get_input_value("gravity"))
        self.state.set_value("air_density_sl", self.get_input_value("air_density_sl"))
        self.state.set_value("cf_yield", self.get_input_value("cf_yield"))
        self.state.set_value("al_yield", self.get_input_value("al_yield"))
        
        sf = self.get_input_value("safety_factor")
        status = "CONSERVATIVE" if sf >= 1.5 else "BASELINE" if sf >= 1.2 else "AGGRESSIVE"
        self.set_output_value("sf_check", status)


# ============================================================================
# SHEET 2: PROPELLER
# ============================================================================

class PropellerTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Propeller Specifications")
        
    def define_equations(self):
        self.add_equation(1, "D_m = D_in × 0.0254", "Convert diameter to meters")
        self.add_equation(2, "P_m = P_in × 0.0254", "Convert pitch to meters")
        self.add_equation(3, "A = π(D/2)²", "Propeller disc area")
        self.add_equation(4, "P/D = P_m / D_m", "Pitch to diameter ratio")
        self.add_equation(5, "C_T = 0.000427 + 0.00144(P/D)", "Thrust coefficient")
        self.add_equation(6, "C_P = C_T × (P/D) × 0.8", "Power coefficient")
        self.add_equation(7, "C_Q = C_P / (2π)", "Torque coefficient")
        self.add_equation(8, "FM = T^(3/2) / (√(2ρA) × P)", "Figure of Merit")
        self.add_equation(9, "DL = T_total / A_total", "Disk loading")
        
    def build_inputs(self):
        self.add_input_field("Diameter", "diameter_in", 30, "inches", 
                           "Propeller diameter", equation_refs=[1, 3], priority=10)
        self.add_input_field("Pitch", "pitch_in", 10, "inches",
                           "Propeller pitch", equation_refs=[2, 4], priority=9)
        self.add_input_field("Number of Blades", "blades", 2, "-", priority=8)
        self.add_input_field("Thrust Coeff (measured)", "ct_measured", 0.10, "-",
                           "Measured C_T if available", equation_refs=[5], priority=7)
        self.add_input_field("Power Coeff (measured)", "cp_measured", 0.08, "-",
                           "Measured C_P if available", equation_refs=[6], priority=6)
        self.add_checkbox("Use Measured Data", "use_measured", False,
                         "Use measured coefficients instead of calculated", priority=5)
        self.add_input_field("Propeller Efficiency", "efficiency", 0.75, "-", priority=4)
        self.add_input_field("Advance Ratio Correction", "j_correction", 0.85, "-", priority=3)
        
    def build_outputs(self):
        self.add_output_field("Diameter", "diameter_m", "m", decimals=4,
                            equation_refs=[1], priority=10)
        self.add_output_field("Pitch", "pitch_m", "m", decimals=4,
                            equation_refs=[2], priority=9)
        self.add_output_field("Disc Area", "area", "m²", decimals=4,
                            equation_refs=[3], priority=8)
        self.add_output_field("P/D Ratio", "pd_ratio", "-", decimals=4,
                            equation_refs=[4], priority=7)
        self.add_output_field("Thrust Coeff (C_T)", "ct", "-", decimals=4,
                            equation_refs=[5], priority=6)
        self.add_output_field("Power Coeff (C_P)", "cp", "-", decimals=4,
                            equation_refs=[6], priority=5)
        self.add_output_field("Torque Coeff (C_Q)", "cq", "-", decimals=4,
                            equation_refs=[7], priority=4)
        self.add_output_field("Figure of Merit", "figure_of_merit", "-", decimals=4,
                            equation_refs=[8], priority=3)
        self.add_output_field("Disk Loading Status", "disk_loading_status", "", decimals=0,
                            equation_refs=[9], priority=2)
        
    def calculate_figure_of_merit(self, thrust, power, rho, area):
        if power <= 0:
            return 0
        P_ideal = thrust * math.sqrt(thrust / (2 * rho * area))
        FM = P_ideal / power
        return max(0, min(1, FM))
        
    def calculate_disk_loading(self, thrust_total, total_area):
        if total_area <= 0:
            return 0
        return thrust_total / total_area
        
    def calculate(self):
        # Unit conversions
        d_m = self.get_input_value("diameter_in") * 0.0254
        p_m = self.get_input_value("pitch_in") * 0.0254
        
        # Geometry
        pd_ratio = p_m / d_m if d_m > 0 else 0
        area = math.pi * (d_m / 2) ** 2
        
        # Coefficients
        use_measured = self.get_input_value("use_measured", bool)
        if use_measured:
            ct = self.get_input_value("ct_measured")
            cp = self.get_input_value("cp_measured")
        else:
            ct = 0.000427 + 0.00144 * pd_ratio
            cp = ct * pd_ratio * 0.8
            
        cq = cp / (2 * math.pi)
        
        # Figure of Merit
        thrust_sample = 200
        power_sample = 1500
        rho = self.state.get_value("air_density_sl", 1.225)
        fm = self.calculate_figure_of_merit(thrust_sample, power_sample, rho, area)
        
        # Disk loading check
        thrust_total_sample = thrust_sample * self.state.get_value("motor_count", 4)
        total_area = area * self.state.get_value("motor_count", 4)
        disk_loading = self.calculate_disk_loading(thrust_total_sample, total_area)
        disk_status = "GOOD" if disk_loading < 300 else "HIGH" if disk_loading < 500 else "VERY HIGH"
        
        # Update outputs
        self.set_output_value("diameter_m", d_m)
        self.set_output_value("pitch_m", p_m)
        self.set_output_value("area", area)
        self.set_output_value("pd_ratio", pd_ratio)
        self.set_output_value("ct", ct)
        self.set_output_value("cp", cp)
        self.set_output_value("cq", cq)
        self.set_output_value("figure_of_merit", fm)
        self.set_output_value("disk_loading_status", disk_status)
        
        # Store in global state
        self.state.set_value("prop_diameter_m", d_m)
        self.state.set_value("prop_area", area)
        self.state.set_value("prop_ct", ct)
        self.state.set_value("prop_cp", cp)
        self.state.set_value("prop_cq", cq)
        self.state.set_value("prop_efficiency", self.get_input_value("efficiency"))
        self.state.set_value("figure_of_merit", fm)


# ============================================================================
# SHEET 3: MOTORS
# ============================================================================

class MotorsTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Motor Specifications")
        
    def define_equations(self):
        self.add_equation(1, "K_t = 60 / (2π × KV)", "Torque constant from KV")
        self.add_equation(2, "m_total = m_motor × N_motors", "Total motor mass")
        
    def build_inputs(self):
        self.add_input_field("Motor KV", "kv", 100, "RPM/V", 
                           equation_refs=[1], priority=10)
        self.add_input_field("Max Continuous Current", "max_current", 80, "A", priority=9)
        self.add_input_field("No-Load Current", "i0", 2, "A", priority=8)
        self.add_input_field("Motor Resistance", "resistance", 0.05, "Ω", priority=7)
        self.add_input_field("Motor Efficiency", "efficiency", 0.85, "-", priority=6)
        self.add_input_field("Max RPM", "max_rpm", 8000, "RPM", priority=5)
        self.add_input_field("Motor Mass", "mass", 0.8, "kg", 
                           equation_refs=[2], priority=4)
        self.add_input_field("Number of Motors", "count", 4, "-", 
                           equation_refs=[2], priority=3)
        
    def build_outputs(self):
        self.add_output_field("Torque Constant (K_t)", "kt", "N·m/A", 
                            equation_refs=[1], priority=10)
        self.add_output_field("Total Motor Mass", "total_mass", "kg", 
                            equation_refs=[2], priority=9)
        
    def calculate(self):
        kv = self.get_input_value("kv")
        kt = 60 / (2 * math.pi * kv) if kv > 0 else 0
        
        count = self.get_input_value("count", int)
        mass = self.get_input_value("mass")
        total_mass = mass * count
        
        self.set_output_value("kt", kt)
        self.set_output_value("total_mass", total_mass)
        
        # Store in state
        self.state.set_value("motor_kv", kv)
        self.state.set_value("motor_kt", kt)
        self.state.set_value("motor_i0", self.get_input_value("i0"))
        self.state.set_value("motor_efficiency", self.get_input_value("efficiency"))
        self.state.set_value("motor_max_rpm", self.get_input_value("max_rpm"))
        self.state.set_value("motor_count", count)
        self.state.set_value("motors_total_mass", total_mass)


# ============================================================================
# SHEET 4: BATTERY
# ============================================================================

class BatteryTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Battery Specifications")
        
    def define_equations(self):
        self.add_equation(1, "V = N_cells × V_cell", "Total battery voltage")
        self.add_equation(2, "E = C_Ah × V", "Energy capacity in Wh")
        self.add_equation(3, "I_max = C_Ah × C_rating", "Maximum current draw")
        self.add_equation(4, "m = E / e_specific", "Battery mass from specific energy")
        self.add_equation(5, "E_usable = E / SF", "Usable energy with safety factor")
        self.add_equation(6, "V_sag = I × R_internal", "Voltage drop under load")
        
    def build_inputs(self):
        self.add_input_field("Capacity", "capacity", 20, "Ah", 
                           equation_refs=[2, 3], priority=10)
        self.add_input_field("C-Rating", "c_rating", 25, "-", 
                           equation_refs=[3], priority=9)
        self.add_input_field("Cells in Series", "cells", 12, "S", 
                           equation_refs=[1], priority=8)
        self.add_input_field("Cell Voltage", "cell_voltage", 3.7, "V", 
                           equation_refs=[1], priority=7)
        self.add_input_field("Specific Energy", "specific_energy", 200, "Wh/kg", 
                           equation_refs=[4], priority=6)
        self.add_input_field("Internal Resistance", "resistance", 0.01, "Ω", 
                           equation_refs=[6], priority=5)
        
    def build_outputs(self):
        self.add_output_field("Battery Voltage", "voltage", "V", 
                            equation_refs=[1], priority=10)
        self.add_output_field("Energy Capacity", "energy", "Wh", 
                            equation_refs=[2], priority=9)
        self.add_output_field("Max Current", "max_current", "A", 
                            equation_refs=[3], priority=8)
        self.add_output_field("Battery Mass", "mass", "kg", 
                            equation_refs=[4], priority=7)
        self.add_output_field("Usable Energy", "usable_energy", "Wh", 
                            equation_refs=[5], priority=6)
        self.add_output_field("Voltage Sag at Max", "voltage_sag", "V", 
                            equation_refs=[6], priority=5)
        
    def calculate_voltage_sag(self, current_draw, nominal_voltage, internal_resistance):
        voltage_drop = current_draw * internal_resistance
        actual_voltage = nominal_voltage - voltage_drop
        return actual_voltage, voltage_drop
        
    def calculate(self):
        cells = self.get_input_value("cells", int)
        cell_v = self.get_input_value("cell_voltage")
        nominal_voltage = cells * cell_v
        
        capacity = self.get_input_value("capacity")
        energy = capacity * nominal_voltage
        
        c_rating = self.get_input_value("c_rating")
        max_current = capacity * c_rating
        
        internal_r = self.get_input_value("resistance")
        voltage_under_load, voltage_sag = self.calculate_voltage_sag(
            max_current, nominal_voltage, internal_r
        )
        
        spec_energy = self.get_input_value("specific_energy")
        mass = energy / spec_energy if spec_energy > 0 else 0
        
        sf = self.state.get_value("safety_factor", 1.2)
        usable_energy = energy / sf
        
        self.set_output_value("voltage", nominal_voltage)
        self.set_output_value("energy", energy)
        self.set_output_value("max_current", max_current)
        self.set_output_value("mass", mass)
        self.set_output_value("usable_energy", usable_energy)
        self.set_output_value("voltage_sag", voltage_sag)
        
        # Store in state
        self.state.set_value("battery_voltage", nominal_voltage)
        self.state.set_value("battery_energy_wh", energy)
        self.state.set_value("battery_capacity", capacity)
        self.state.set_value("battery_max_current", max_current)
        self.state.set_value("battery_mass", mass)
        self.state.set_value("battery_usable_wh", usable_energy)
        self.state.set_value("battery_resistance", internal_r)
        self.state.set_value("battery_voltage_loaded", voltage_under_load)


# ============================================================================
# SHEET 5: AIRFRAME
# ============================================================================

class AirframeTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Airframe Structure & Mass")
        
    def define_equations(self):
        self.add_equation(1, "D_inner = D_outer - 2t", "Arm inner diameter")
        self.add_equation(2, "A = π/4 (D_o² - D_i²)", "Arm cross-section area")
        self.add_equation(3, "m_empty = Σ m_components", "Total empty mass")
        
    def build_inputs(self):
        self.add_input_field("Frame Mass", "frame_mass", 6.0, "kg", 
                           equation_refs=[3], priority=10)
        self.add_input_field("Avionics Mass", "avionics_mass", 0.6, "kg", 
                           equation_refs=[3], priority=9)
        self.add_input_field("ESC Mass", "esc_mass", 1.2, "kg", 
                           equation_refs=[3], priority=8)
        self.add_input_field("Landing Gear Mass", "gear_mass", 0.6, "kg", 
                           equation_refs=[3], priority=7)
        self.add_input_field("Payload Mount Mass", "mount_mass", 0.4, "kg", 
                           equation_refs=[3], priority=6)
        self.add_input_field("Misc Mass", "misc_mass", 0.5, "kg", 
                           equation_refs=[3], priority=5)
        self.add_input_field("Arm Length", "arm_length", 1.0, "m", priority=4)
        self.add_input_field("Arm Outer Diameter", "arm_od", 30, "mm", 
                           equation_refs=[1, 2], priority=3)
        self.add_input_field("Arm Wall Thickness", "arm_thickness", 2, "mm", 
                           equation_refs=[1], priority=2)
        
    def build_outputs(self):
        self.add_output_field("Arm Inner Diameter", "arm_id", "mm", 
                            equation_refs=[1], priority=10)
        self.add_output_field("Arm Cross-Section", "arm_area", "m²", decimals=6,
                            equation_refs=[2], priority=9)
        self.add_output_field("Drone Empty Mass", "empty_mass", "kg", 
                            equation_refs=[3], priority=8)
        self.add_output_field("Weight Check", "weight_check", "", decimals=0, priority=7)
        
    def calculate(self):
        # Arm geometry
        od = self.get_input_value("arm_od")
        thickness = self.get_input_value("arm_thickness")
        id_mm = od - 2 * thickness
        
        arm_area = math.pi * ((od/1000)**2 - (id_mm/1000)**2) / 4
        
        # Total empty mass
        empty_mass = (self.get_input_value("frame_mass") +
                     self.get_input_value("avionics_mass") +
                     self.get_input_value("esc_mass") +
                     self.get_input_value("gear_mass") +
                     self.get_input_value("mount_mass") +
                     self.get_input_value("misc_mass") +
                     self.state.get_value("motors_total_mass", 0) +
                     self.state.get_value("battery_mass", 0))
        
        weight_check = "PASS" if empty_mass <= 24.95 else "FAIL"
        
        self.set_output_value("arm_id", id_mm)
        self.set_output_value("arm_area", arm_area)
        self.set_output_value("empty_mass", empty_mass)
        self.set_output_value("weight_check", weight_check)
        
        self.state.set_value("drone_empty_mass", empty_mass)
        self.state.set_value("arm_length", self.get_input_value("arm_length"))


# ============================================================================
# REMAINING TABS (6-13) - Abbreviated for space
# ============================================================================

class FixedWingTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Fixed Wing Parameters")
        
    def define_equations(self):
        self.add_equation(1, "b = √(AR × S)", "Wing span")
        self.add_equation(2, "c = S / b", "Wing chord")
        
    def build_inputs(self):
        self.add_input_field("Wing Area", "wing_area", 2.0, "m²", 
                           equation_refs=[1, 2], priority=10)
        self.add_input_field("Aspect Ratio", "aspect_ratio", 8, "-", 
                           equation_refs=[1], priority=9)
        self.add_input_field("Max Lift Coefficient", "cl_max", 1.4, "-", priority=8)
        self.add_input_field("Cruise Lift Coefficient", "cl_cruise", 0.6, "-", priority=7)
        self.add_input_field("Parasitic Drag Coeff", "cd0", 0.025, "-", priority=6)
        self.add_input_field("Oswald Efficiency", "oswald_e", 0.85, "-", priority=5)
        self.add_checkbox("Use Wing in Cruise", "use_wing", True, priority=4)
        
    def build_outputs(self):
        self.add_output_field("Wing Span", "span", "m", equation_refs=[1], priority=10)
        self.add_output_field("Wing Chord", "chord", "m", equation_refs=[2], priority=9)
        
    def calculate(self):
        area = self.get_input_value("wing_area")
        ar = self.get_input_value("aspect_ratio")
        
        span = math.sqrt(ar * area)
        chord = area / span if span > 0 else 0
        
        self.set_output_value("span", span)
        self.set_output_value("chord", chord)
        
        self.state.set_value("wing_area", area)
        self.state.set_value("wing_ar", ar)
        self.state.set_value("wing_cl_max", self.get_input_value("cl_max"))
        self.state.set_value("wing_cd0", self.get_input_value("cd0"))
        self.state.set_value("wing_e", self.get_input_value("oswald_e"))
        self.state.set_value("use_wing", self.get_input_value("use_wing", bool))


class MissionProfileTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Mission Profile")
        
    def define_equations(self):
        self.add_equation(1, "m_total = m_empty + m_payload", "Total loaded mass")
        self.add_equation(2, "W = m_total × g", "Total weight")
        self.add_equation(3, "PWR = m_payload / m_empty", "Payload weight ratio")
        
    def build_inputs(self):
        self.add_input_field("Payload Mass", "payload_mass", 49.9, "kg", 
                           equation_refs=[1, 3], priority=10)
        self.add_input_field("Total Distance", "distance", 9260, "m", priority=9)
        self.add_input_field("Distance Loaded", "distance_loaded", 7408, "m", priority=8)
        self.add_input_field("Distance Unloaded", "distance_unloaded", 1852, "m", priority=7)
        self.add_input_field("Cruise Altitude", "altitude", 107, "m", priority=6)
        self.add_input_field("Number of 180° Turns", "turns_180", 0, "-", priority=5)
        self.add_input_field("Number of 90° Turns", "turns_90", 0, "-", priority=4)
        self.add_input_field("Number of 45° Turns", "turns_45", 0, "-", priority=3)
        self.add_input_field("Cruise Speed", "speed", 25, "m/s", priority=2)
        self.add_input_field("Max Bank Angle", "bank_angle", 30, "degrees", priority=1)
        self.add_input_field("Hover Takeoff Time", "hover_to", 30, "s", priority=0)
        self.add_input_field("Hover Landing Time", "hover_land", 30, "s", priority=0)
        self.add_input_field("Transition Time", "transition", 10, "s", priority=0)
        
    def build_outputs(self):
        self.add_output_field("Total Mass (Loaded)", "total_mass_loaded", "kg", 
                            equation_refs=[1], priority=10)
        self.add_output_field("Total Weight (Loaded)", "total_weight_loaded", "N", 
                            equation_refs=[2], priority=9)
        self.add_output_field("Total Mass (Unloaded)", "total_mass_unloaded", "kg", priority=8)
        self.add_output_field("Payload Ratio", "payload_ratio", "-", 
                            equation_refs=[3], priority=7)
        
    def calculate(self):
        payload = self.get_input_value("payload_mass")
        empty = self.state.get_value("drone_empty_mass", 24.95)
        g = self.state.get_value("gravity", 9.81)
        
        total_loaded = empty + payload
        weight_loaded = total_loaded * g
        payload_ratio = payload / empty if empty > 0 else 0
        
        self.set_output_value("total_mass_loaded", total_loaded)
        self.set_output_value("total_weight_loaded", weight_loaded)
        self.set_output_value("total_mass_unloaded", empty)
        self.set_output_value("payload_ratio", payload_ratio)
        
        self.state.set_value("payload_mass", payload)
        self.state.set_value("total_mass_loaded", total_loaded)
        self.state.set_value("total_weight_loaded", weight_loaded)
        self.state.set_value("total_mass_unloaded", empty)
        self.state.set_value("mission_speed", self.get_input_value("speed"))
        self.state.set_value("payload_ratio", payload_ratio)
        self.state.set_value("transition_time", self.get_input_value("transition"))


class WeatherTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Environmental Conditions")
        
    def define_equations(self):
        self.add_equation(1, "ρ = P / (R × T)", "Air density from ideal gas law")
        self.add_equation(2, "V_net = max(0, V_head - V_tail)", "Net headwind")
        self.add_equation(3, "σ = ρ / ρ_SL", "Density ratio")
        
    def build_inputs(self):
        self.add_input_field("Headwind", "headwind", 0, "m/s", 
                           equation_refs=[2], priority=10)
        self.add_input_field("Tailwind", "tailwind", 0, "m/s", 
                           equation_refs=[2], priority=9)
        self.add_input_field("Crosswind", "crosswind", 0, "m/s", priority=8)
        self.add_input_field("Temperature", "temperature", 15, "°C", 
                           equation_refs=[1], priority=7)
        self.add_input_field("Pressure", "pressure", 101325, "Pa", 
                           equation_refs=[1], priority=6)
        self.add_input_field("Gust Velocity", "gust", 5, "m/s", priority=5)
        
    def build_outputs(self):
        self.add_output_field("Air Density", "air_density", "kg/m³", 
                            equation_refs=[1], priority=10)
        self.add_output_field("Net Headwind", "net_wind", "m/s", 
                            equation_refs=[2], priority=9)
        self.add_output_field("Density Ratio", "density_ratio", "-", 
                            equation_refs=[3], priority=8)
        
    def calculate(self):
        temp_c = self.get_input_value("temperature")
        pressure = self.get_input_value("pressure")
        
        temp_k = temp_c + 273.15
        air_density = pressure / (287.05 * temp_k)
        
        headwind = self.get_input_value("headwind")
        tailwind = self.get_input_value("tailwind")
        net_wind = max(0, headwind - tailwind)
        
        rho_sl = self.state.get_value("air_density_sl", 1.225)
        density_ratio = air_density / rho_sl
        
        self.set_output_value("air_density", air_density)
        self.set_output_value("net_wind", net_wind)
        self.set_output_value("density_ratio", density_ratio)
        
        self.state.set_value("air_density_actual", air_density)
        self.state.set_value("wind_net", net_wind)


class HoverPerformanceTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Hover Performance Calculations")
        
    def define_equations(self):
        self.add_equation(1, "T_req = W × SF", "Required thrust with safety factor")
        self.add_equation(2, "RPM = 60√(T / (C_T × ρ × D⁴))", "Required RPM")
        self.add_equation(3, "Q = C_Q × ρ × n² × D⁵", "Motor torque")
        self.add_equation(4, "P = C_P × ρ × n³ × D⁵", "Mechanical power")
        self.add_equation(5, "I = I_0 + Q / K_t", "Motor current")
        self.add_equation(6, "V_tip = π × D × RPM / 60", "Propeller tip speed")
        self.add_equation(7, "M = V_tip / V_sound", "Tip Mach number")
        self.add_equation(8, "P_IGE = P_OGE × (1 - k×D/h)", "Ground effect power reduction")
        
    def build_inputs(self):
        self.add_input_field("Ground Effect Altitude", "ground_effect_alt", 1.0, "m",
                           "Height above ground", equation_refs=[8], priority=10)
        
    def build_outputs(self):
        self.add_output_field("Required Total Thrust", "thrust_total", "N", 
                            equation_refs=[1], priority=10)
        self.add_output_field("Thrust Per Motor", "thrust_per_motor", "N", priority=9)
        self.add_output_field("Required RPM", "rpm", "RPM", 
                            equation_refs=[2], priority=8)
        self.add_output_field("RPM Check", "rpm_check", "", decimals=0, priority=7)
        self.add_output_field("Torque Per Motor", "torque", "N·m", 
                            equation_refs=[3], priority=6)
        self.add_output_field("Mech Power Per Motor", "power_mech", "W", 
                            equation_refs=[4], priority=5)
        self.add_output_field("Elec Power Per Motor", "power_elec", "W", priority=4)
        self.add_output_field("Total Hover Power", "power_total", "W", priority=3)
        self.add_output_field("Current Per Motor", "current_motor", "A", 
                            equation_refs=[5], priority=2)
        self.add_output_field("Total Hover Current", "current_total", "A", priority=1)
        self.add_output_field("Current Check", "current_check", "", decimals=0, priority=0)
        self.add_output_field("Hover Time Available", "hover_time", "min", priority=0)
        self.add_output_field("Tip Speed", "tip_speed", "m/s", 
                            equation_refs=[6], priority=0)
        self.add_output_field("Tip Mach Number", "tip_mach", "-", 
                            equation_refs=[7], priority=0)
        self.add_output_field("Ground Effect Benefit", "ground_effect_pct", "%", 
                            equation_refs=[8], priority=0)
        
    def calculate_tip_speed(self, rpm, diameter):
        tip_speed = math.pi * diameter * rpm / 60
        speed_of_sound = self.state.get_value("speed_of_sound", 343)
        mach_number = tip_speed / speed_of_sound
        return tip_speed, mach_number
        
    def calculate_ground_effect(self, altitude_agl, diameter):
        if altitude_agl <= 0:
            return 1.0, 0.0
        if altitude_agl < diameter:
            reduction = (diameter - altitude_agl) / diameter * 0.15
            return (1 - reduction), reduction * 100
        return 1.0, 0.0
        
    def calculate_voltage_sag(self, current_total, nominal_voltage, internal_resistance):
        voltage_drop = current_total * internal_resistance
        actual_voltage = nominal_voltage - voltage_drop
        return max(0, actual_voltage), voltage_drop
        
    def calculate(self):
        weight = self.state.get_value("total_weight_loaded", 735)
        sf = self.state.get_value("safety_factor", 1.2)
        n_motors = self.state.get_value("motor_count", 4)
        rho = self.state.get_value("air_density_actual", 1.225)
        ct = self.state.get_value("prop_ct", 0.10)
        cp = self.state.get_value("prop_cp", 0.08)
        cq = self.state.get_value("prop_cq", 0.0127)
        d = self.state.get_value("prop_diameter_m", 0.762)
        motor_eff = self.state.get_value("motor_efficiency", 0.85)
        kt = self.state.get_value("motor_kt", 0.0955)
        i0 = self.state.get_value("motor_i0", 2)
        motor_max_rpm = self.state.get_value("motor_max_rpm", 8000)
        batt_max_current = self.state.get_value("battery_max_current", 500)
        batt_capacity = self.state.get_value("battery_capacity", 20)
        batt_voltage = self.state.get_value("battery_voltage", 44.4)
        batt_resistance = self.state.get_value("battery_resistance", 0.01)
        
        thrust_total = weight * sf
        thrust_per_motor = thrust_total / n_motors if n_motors > 0 else 0
        
        if ct > 0 and rho > 0 and d > 0:
            rpm = 60 * math.sqrt(thrust_per_motor / (ct * rho * d**4))
        else:
            rpm = 0
            
        rpm_check = "PASS" if rpm < motor_max_rpm else "FAIL"
        
        tip_speed, tip_mach = self.calculate_tip_speed(rpm, d)
        tip_mach_check = "PASS" if tip_mach < 0.6 else "WARNING"
        
        ground_alt = self.get_input_value("ground_effect_alt")
        ge_factor, ge_benefit = self.calculate_ground_effect(ground_alt, d)
        power_multiplier = ge_factor
        
        n_rps = rpm / 60
        torque = cq * rho * (n_rps**2) * (d**5) if n_rps > 0 else 0
        power_mech = cp * rho * (n_rps**3) * (d**5) * power_multiplier if n_rps > 0 else 0
        power_elec = power_mech / motor_eff if motor_eff > 0 else 0
        power_total = power_elec * n_motors
        
        current_motor = i0 + (torque / kt) if kt > 0 else 0
        current_total = current_motor * n_motors
        
        actual_voltage, voltage_drop = self.calculate_voltage_sag(
            current_total, batt_voltage, batt_resistance
        )
        
        if actual_voltage > 0:
            power_elec = current_motor * actual_voltage
            power_total = power_elec * n_motors
        
        current_check = "PASS" if current_total < batt_max_current else "FAIL"
        hover_time = (batt_capacity * 60) / current_total if current_total > 0 else 0
        
        self.set_output_value("thrust_total", thrust_total)
        self.set_output_value("thrust_per_motor", thrust_per_motor)
        self.set_output_value("rpm", rpm)
        self.set_output_value("rpm_check", rpm_check)
        self.set_output_value("torque", torque)
        self.set_output_value("power_mech", power_mech)
        self.set_output_value("power_elec", power_elec)
        self.set_output_value("power_total", power_total)
        self.set_output_value("current_motor", current_motor)
        self.set_output_value("current_total", current_total)
        self.set_output_value("current_check", current_check)
        self.set_output_value("hover_time", hover_time)
        self.set_output_value("tip_speed", tip_speed)
        self.set_output_value("tip_mach", f"{tip_mach:.3f} {tip_mach_check}")
        self.set_output_value("ground_effect_pct", ge_benefit)
        
        self.state.set_value("hover_power_total", power_total)
        self.state.set_value("hover_current_total", current_total)
        self.state.set_value("tip_mach_number", tip_mach)
        self.state.set_value("thrust_total", thrust_total)


class CruisePerformanceTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Fixed-Wing Cruise Performance")
        
    def define_equations(self):
        self.add_equation(1, "C_L = W / (0.5 × ρ × V² × S)", "Required lift coefficient")
        self.add_equation(2, "C_Di = C_L² / (π × AR × e)", "Induced drag coefficient")
        self.add_equation(3, "D = 0.5 × ρ × V² × S × C_D", "Total drag force")
        self.add_equation(4, "L/D = C_L / C_D", "Lift-to-drag ratio")
        
    def build_inputs(self):
        pass
        
    def build_outputs(self):
        self.add_output_field("Groundspeed", "groundspeed", "m/s", priority=10)
        self.add_output_field("Required C_L (Loaded)", "cl_required", "-", 
                            equation_refs=[1], priority=9)
        self.add_output_field("Induced Drag Coeff", "cdi", "-", 
                            equation_refs=[2], priority=8)
        self.add_output_field("Total Drag Coeff", "cd_total", "-", priority=7)
        self.add_output_field("Drag (Loaded)", "drag", "N", 
                            equation_refs=[3], priority=6)
        self.add_output_field("L/D Ratio", "ld_ratio", "-", 
                            equation_refs=[4], priority=5)
        self.add_output_field("Cruise Power (Loaded)", "cruise_power", "W", priority=4)
        self.add_output_field("Cruise Current (Loaded)", "cruise_current", "A", priority=3)
        
    def calculate(self):
        speed = self.state.get_value("mission_speed", 25)
        wind_net = self.state.get_value("wind_net", 0)
        weight = self.state.get_value("total_weight_loaded", 735)
        rho = self.state.get_value("air_density_actual", 1.225)
        s = self.state.get_value("wing_area", 2.0)
        ar = self.state.get_value("wing_ar", 8)
        cd0 = self.state.get_value("wing_cd0", 0.025)
        e = self.state.get_value("wing_e", 0.85)
        use_wing = self.state.get_value("use_wing", True)
        prop_eff = self.state.get_value("prop_efficiency", 0.75)
        motor_eff = self.state.get_value("motor_efficiency", 0.85)
        batt_v = self.state.get_value("battery_voltage", 44.4)
        n_motors = self.state.get_value("motor_count", 4)
        
        groundspeed = speed
        cl_required = weight / (0.5 * rho * speed**2 * s) if (rho * speed**2 * s) > 0 else 0
        cdi = (cl_required**2) / (math.pi * ar * e) if (ar * e) > 0 else 0
        cd_total = cd0 + cdi
        drag = 0.5 * rho * speed**2 * s * cd_total
        ld_ratio = cl_required / cd_total if cd_total > 0 else 0
        
        if use_wing:
            thrust = drag
        else:
            thrust = weight
            
        cruise_power = (thrust * speed) / (prop_eff * motor_eff) if (prop_eff * motor_eff) > 0 else 0
        cruise_power_total = cruise_power * n_motors
        cruise_current = cruise_power_total / batt_v if batt_v > 0 else 0
        
        self.set_output_value("groundspeed", groundspeed)
        self.set_output_value("cl_required", cl_required)
        self.set_output_value("cdi", cdi)
        self.set_output_value("cd_total", cd_total)
        self.set_output_value("drag", drag)
        self.set_output_value("ld_ratio", ld_ratio)
        self.set_output_value("cruise_power", cruise_power_total)
        self.set_output_value("cruise_current", cruise_current)
        
        self.state.set_value("cruise_power_loaded", cruise_power_total)
        self.state.set_value("ld_ratio", ld_ratio)


class TurnAnalysisTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Turn Energy Analysis")
        
    def define_equations(self):
        self.add_equation(1, "R = V² / (g × tan(φ))", "Turn radius")
        self.add_equation(2, "n = 1 / cos(φ)", "Load factor")
        self.add_equation(3, "L_arc = R × θ", "Arc length")
        
    def build_inputs(self):
        pass
        
    def build_outputs(self):
        self.add_output_field("Turn Radius", "turn_radius", "m", 
                            equation_refs=[1], priority=10)
        self.add_output_field("Load Factor", "load_factor", "-", 
                            equation_refs=[2], priority=9)
        self.add_output_field("180° Turn Arc", "arc_180", "m", 
                            equation_refs=[3], priority=8)
        self.add_output_field("90° Turn Arc", "arc_90", "m", 
                            equation_refs=[3], priority=7)
        self.add_output_field("Energy per 180° Turn", "energy_180", "Wh", priority=6)
        self.add_output_field("Total Turn Energy", "total_turn_energy", "Wh", priority=5)
        
    def calculate(self):
        self.set_output_value("turn_radius", 0)
        self.set_output_value("load_factor", 1.0)
        self.set_output_value("arc_180", 0)
        self.set_output_value("arc_90", 0)
        self.set_output_value("energy_180", 0)
        self.set_output_value("total_turn_energy", 0)
        
        self.state.set_value("turn_energy_total", 0)


class EnergyBudgetTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Mission Energy Budget")
        
    def define_equations(self):
        self.add_equation(1, "E_hover = P_hover × t_hover / 3600", "Hover energy (Wh)")
        self.add_equation(2, "E_cruise = P_cruise × t_cruise / 3600", "Cruise energy (Wh)")
        self.add_equation(3, "E_transition = P_trans × t_trans / 3600", "Transition energy")
        self.add_equation(4, "E_total = E_hover + E_cruise + E_trans + E_turn", "Total mission energy")
        self.add_equation(5, "E_reserve = E_total × SF", "Energy with reserve")
        self.add_equation(6, "Margin = (E_avail - E_reserve) / E_reserve × 100%", "Energy margin")
        
    def build_inputs(self):
        self.add_input_field("Transition Power Multiplier", "transition_multiplier", 1.2, "-",
                           "Power increase during transition", equation_refs=[3], priority=10)
        
    def build_outputs(self):
        self.add_output_field("Hover Energy", "hover_energy", "Wh", 
                            equation_refs=[1], priority=10)
        self.add_output_field("Cruise Energy", "cruise_energy", "Wh", 
                            equation_refs=[2], priority=9)
        self.add_output_field("Transition Energy", "transition_energy", "Wh", 
                            equation_refs=[3], priority=8)
        self.add_output_field("Turn Energy", "turn_energy", "Wh", priority=7)
        self.add_output_field("Total Energy", "total_energy", "Wh", 
                            equation_refs=[4], priority=6)
        self.add_output_field("With Reserve", "with_reserve", "Wh", 
                            equation_refs=[5], priority=5)
        self.add_output_field("Available Energy", "available", "Wh", priority=4)
        self.add_output_field("Energy Margin", "margin", "%", 
                            equation_refs=[6], priority=3)
        self.add_output_field("Energy Check", "check", "", decimals=0, priority=2)
        
    def calculate_transition_energy(self):
        transition_time = self.state.get_value("transition_time", 10)
        hover_power = self.state.get_value("hover_power_total", 6000)
        cruise_power = self.state.get_value("cruise_power_loaded", 1800)
        transition_mult = self.get_input_value("transition_multiplier")
        
        p_transition = max(hover_power, cruise_power) * transition_mult
        energy_transition = (p_transition * transition_time) / 3600
        return energy_transition
        
    def calculate(self):
        hover_power = self.state.get_value("hover_power_total", 6000)
        cruise_power = self.state.get_value("cruise_power_loaded", 1800)
        sf = self.state.get_value("safety_factor", 1.2)
        batt_usable = self.state.get_value("battery_usable_wh", 888)
        
        hover_energy = (hover_power * 60) / 3600
        cruise_energy = (cruise_power * 400) / 3600
        transition_energy = self.calculate_transition_energy()
        turn_energy = self.state.get_value("turn_energy_total", 0)
        
        total = hover_energy + cruise_energy + transition_energy + turn_energy
        with_reserve = total * sf
        
        margin = ((batt_usable - with_reserve) / with_reserve) * 100 if with_reserve > 0 else 0
        check = "PASS" if batt_usable > with_reserve else "FAIL"
        
        self.set_output_value("hover_energy", hover_energy)
        self.set_output_value("cruise_energy", cruise_energy)
        self.set_output_value("transition_energy", transition_energy)
        self.set_output_value("turn_energy", turn_energy)
        self.set_output_value("total_energy", total)
        self.set_output_value("with_reserve", with_reserve)
        self.set_output_value("available", batt_usable)
        self.set_output_value("margin", margin)
        self.set_output_value("check", check)
        
        self.state.set_value("energy_margin_pct", margin)
        self.state.set_value("transition_energy", transition_energy)


class PerformanceSummaryTab(BaseTab):
    def __init__(self, state):
        super().__init__(state, "Performance Summary Dashboard")
        
    def define_equations(self):
        self.add_equation(1, "PWR = m_payload / m_empty", "Payload weight ratio")
        self.add_equation(2, "DL = T_total / A_total", "Disk loading")
        
    def build_inputs(self):
        pass
        
    def build_outputs(self):
        self.add_output_field("Payload Ratio", "payload_ratio", "-", 
                            equation_refs=[1], priority=10)
        self.add_output_field("Status", "status", "", decimals=0, priority=9)
        self.add_output_field("Empty Weight", "empty_weight", "kg", priority=8)
        self.add_output_field("Weight Check", "weight_check", "", decimals=0, priority=7)
        self.add_output_field("Energy Margin", "energy_margin", "%", priority=6)
        self.add_output_field("Energy Check", "energy_check", "", decimals=0, priority=5)
        self.add_output_field("L/D Ratio", "ld_ratio", "-", priority=4)
        self.add_output_field("L/D Status", "ld_status", "", decimals=0, priority=3)
        self.add_output_field("Disk Loading", "disk_loading", "N/m²", 
                            equation_refs=[2], priority=2)
        self.add_output_field("Figure of Merit", "figure_of_merit", "-", priority=1)
        self.add_output_field("Tip Mach Number", "tip_mach", "-", priority=0)
        
    def calculate_disk_loading(self):
        thrust_total = self.state.get_value("thrust_total", 0)
        prop_area = self.state.get_value("prop_area", 1)
        n_motors = self.state.get_value("motor_count", 4)
        
        total_area = prop_area * n_motors
        return thrust_total / total_area if total_area > 0 else 0
        
    def calculate(self):
        ratio = self.state.get_value("payload_ratio", 0)
        empty = self.state.get_value("drone_empty_mass", 25)
        energy_margin = self.state.get_value("energy_margin_pct", 0)
        ld = self.state.get_value("ld_ratio", 0)
        fm = self.state.get_value("figure_of_merit", 0)
        tip_mach = self.state.get_value("tip_mach_number", 0)
        
        disk_loading = self.calculate_disk_loading()
        
        if ratio >= 4:
            status = "COMPETITIVE"
        elif ratio >= 2:
            status = "QUALIFYING"
        else:
            status = "BELOW MINIMUM"
            
        weight_check = "PASS" if empty <= 24.95 else "FAIL"
        energy_check = "PASS" if energy_margin > 0 else "FAIL"
        ld_status = "GOOD" if ld > 8 else "LOW"
        fm_status = "GOOD" if fm > 0.6 else "POOR"
        mach_status = "SAFE" if tip_mach < 0.6 else "HIGH"
        
        self.set_output_value("payload_ratio", ratio)
        self.set_output_value("status", status)
        self.set_output_value("empty_weight", empty)
        self.set_output_value("weight_check", weight_check)
        self.set_output_value("energy_margin", energy_margin)
        self.set_output_value("energy_check", energy_check)
        self.set_output_value("ld_ratio", ld)
        self.set_output_value("ld_status", ld_status)
        self.set_output_value("disk_loading", disk_loading)
        self.set_output_value("figure_of_merit", f"{fm:.3f} ({fm_status})")
        self.set_output_value("tip_mach", f"{tip_mach:.3f} ({mach_status})")