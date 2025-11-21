# VTOL Drone Design Calculator - Complete Spreadsheet Setup Guide

## Overview

This guide walks you through creating a multi-sheet Excel workbook for designing a VTOL drone for the DARPA Lift Challenge. Each sheet handles a specific subsystem, with cross-references linking everything together.

---

## SHEET STRUCTURE

| Sheet Number | Sheet Name | Purpose |
|--------------|------------|---------|
| 1 | Constants & Safety | Global constants, safety factors, material properties |
| 2 | Propeller | Propeller geometry and performance coefficients |
| 3 | Motors | Motor specifications and torque-speed relationships |
| 4 | Battery | Battery capacity, voltage, discharge characteristics |
| 5 | Airframe | Structural mass breakdown and arm sizing |
| 6 | Fixed Wing | Wing aerodynamics (if using hybrid design) |
| 7 | Mission Profile | Payload, distances, turns, speed targets |
| 8 | Weather | Environmental conditions affecting performance |
| 9 | Hover Performance | All hover-related calculations |
| 10 | Cruise Performance | Fixed-wing cruise calculations |
| 11 | Turn Analysis | Energy cost of maneuvers |
| 12 | Energy Budget | Complete mission energy breakdown |
| 13 | Performance Summary | Dashboard with pass/fail checks |

---

# SHEET 1: CONSTANTS & SAFETY

## Purpose
Central location for all global constants and safety factors. Changing values here automatically updates all dependent calculations throughout the workbook.

## Variables to Enter

### Safety Factor (Single Universal Value)
| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Safety_Factor | SF | 1.2 | - | Universal safety margin (+20% for everything) |

**This single value is applied to:**
- Thrust requirements (need 20% more thrust than weight)
- Structural loads (design for 20% higher loads)
- Energy reserves (carry 20% more energy than calculated)
- Battery capacity planning (size battery 20% larger)
- Current limits (stay 20% below max ratings)

### Physical Constants
| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Gravity | g | 9.81 | m/s² | Gravitational acceleration |
| AirDensity_SL | ρ_0 | 1.225 | kg/m³ | Air density at sea level |
| SpeedOfSound | a | 343 | m/s | Speed of sound at sea level |

### Material Properties
| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| CF_Yield | σ_cf | 600 | MPa | Carbon fiber yield strength |
| CF_Density | ρ_cf | 1600 | kg/m³ | Carbon fiber density |
| AL_Yield | σ_al | 276 | MPa | Aluminum 6061-T6 yield strength |
| AL_Density | ρ_al | 2700 | kg/m³ | Aluminum density |

### Competition Limits
| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Time_Limit | t_max | 30 | min | Maximum mission time |
| Drone_Weight_Limit | m_limit | 24.95 | kg | 55 lb limit |
| Min_Payload | m_pay_min | 49.9 | kg | 110 lb minimum |

---

# SHEET 2: PROPELLER

## Purpose
Define propeller geometry and calculate/store performance coefficients.

## Input Variables

| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Prop_Diameter_in | D_in | 30 | inches | Propeller diameter |
| Prop_Pitch_in | P_in | 10 | inches | Propeller pitch |
| Prop_Blades | n_blades | 2 | - | Number of blades |
| Prop_CT_Measured | C_T_meas | - | - | Thrust coefficient (if from test data) |
| Prop_CP_Measured | C_P_meas | - | - | Power coefficient (if from test data) |
| Prop_UseMeasured | Use_Data | FALSE | - | Toggle: use measured vs estimated |
| Prop_Efficiency | η_prop | 0.75 | - | Propeller efficiency |

## Calculated Variables

### Unit Conversions
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Prop_Diameter_m | = Prop_Diameter_in × 0.0254 | m |
| Prop_Pitch_m | = Prop_Pitch_in × 0.0254 | m |

### Geometry
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Prop_PD_Ratio | = Prop_Pitch_m ÷ Prop_Diameter_m | - |
| Prop_Area | = π × (Prop_Diameter_m ÷ 2)² | m² |

### Coefficient Estimation (when measured data unavailable)
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Prop_CT_Est | = 0.000427 + 0.00144 × Prop_PD_Ratio | - |
| Prop_CP_Est | = Prop_CT_Est × Prop_PD_Ratio × 0.8 | - |

### Final Coefficients (switches based on Use_Data toggle)
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Prop_CT | = IF(Prop_UseMeasured, Prop_CT_Measured, Prop_CT_Est) | - |
| Prop_CP | = IF(Prop_UseMeasured, Prop_CP_Measured, Prop_CP_Est) | - |
| Prop_CQ | = Prop_CP ÷ (2 × π) | - |

---

# SHEET 3: MOTORS

## Purpose
Define motor characteristics and calculate torque-speed relationships.

## Input Variables

| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Motor_KV | KV | 100 | RPM/V | Motor velocity constant |
| Motor_MaxCurrent | I_max | 80 | A | Max continuous current |
| Motor_I0 | I_0 | 2 | A | No-load current |
| Motor_Resistance | R_m | 0.05 | Ω | Motor winding resistance |
| Motor_Efficiency | η_motor | 0.85 | - | Motor efficiency |
| Motor_MaxRPM | RPM_max | 8000 | RPM | Maximum safe RPM |
| Motor_Mass | m_motor | 0.8 | kg | Mass per motor |
| Motor_Count | N_motors | 4 | - | Number of lift motors |

## Calculated Variables

| Variable Name | Equation | Units |
|---------------|----------|-------|
| Motor_Kt | = 60 ÷ (2 × π × Motor_KV) | N·m/A |
| Motor_RPM_NoLoad | = Motor_KV × Battery_Voltage | RPM |
| Motors_TotalMass | = Motor_Mass × Motor_Count | kg |

---

# SHEET 4: BATTERY

## Purpose
Define battery pack specifications and calculate available energy.

## Input Variables

| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Battery_Capacity_Ah | C | 20 | Ah | Battery capacity |
| Battery_CRating | C_rate | 25 | - | Continuous discharge rate |
| Battery_Cells | N_s | 12 | S | Number of cells in series |
| Battery_CellVoltage | V_cell | 3.7 | V | Nominal voltage per cell |
| Battery_SpecificEnergy | e_spec | 200 | Wh/kg | Energy density |
| Battery_Resistance | R_int | 0.01 | Ω | Internal resistance |

## Calculated Variables

| Variable Name | Equation | Units |
|---------------|----------|-------|
| Battery_Voltage | = Battery_Cells × Battery_CellVoltage | V |
| Battery_Energy_Wh | = Battery_Capacity_Ah × Battery_Voltage | Wh |
| Battery_MaxCurrent | = Battery_Capacity_Ah × Battery_CRating | A |
| Battery_Mass | = Battery_Energy_Wh ÷ Battery_SpecificEnergy | kg |
| Battery_Usable_Wh | = Battery_Energy_Wh ÷ Safety_Factor | Wh |

---

# SHEET 5: AIRFRAME

## Purpose
Track all component masses and structural sizing.

## Input Variables

| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Frame_Mass | m_frame | 6.0 | kg | Primary structure mass |
| Avionics_Mass | m_avionics | 0.6 | kg | Flight controller, sensors |
| ESC_Mass | m_esc | 1.2 | kg | ESCs + wiring |
| LandingGear_Mass | m_gear | 0.6 | kg | Landing gear |
| PayloadMount_Mass | m_mount | 0.4 | kg | Payload attachment |
| Misc_Mass | m_misc | 0.5 | kg | Fasteners, connectors |
| Arm_Length | L_arm | 1.0 | m | Motor arm length |
| Arm_OD | D_out | 30 | mm | Arm tube outer diameter |
| Arm_Thickness | t_wall | 2 | mm | Arm tube wall thickness |

## Calculated Variables

| Variable Name | Equation | Units |
|---------------|----------|-------|
| Arm_ID | = Arm_OD - 2 × Arm_Thickness | mm |
| Arm_Area | = π × ((Arm_OD/1000)² - (Arm_ID/1000)²) ÷ 4 | m² |
| Arm_I | = π × ((Arm_OD/1000)⁴ - (Arm_ID/1000)⁴) ÷ 64 | m⁴ |
| Arm_SectionModulus | = Arm_I ÷ (Arm_OD/2000) | m³ |
| Drone_EmptyMass | = Frame_Mass + Avionics_Mass + ESC_Mass + LandingGear_Mass + PayloadMount_Mass + Misc_Mass + Motors_TotalMass + Battery_Mass | kg |

### Structural Check
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Load_Per_Arm | = (Payload_Mass × Gravity × Safety_Factor) ÷ Motor_Count | N |
| Moment_At_Root | = Load_Per_Arm × Arm_Length + Motor_Mass × Gravity × Safety_Factor × Arm_Length | N·m |
| Stress_Max | = Moment_At_Root ÷ Arm_SectionModulus | Pa |
| Stress_Check | = IF(Stress_Max < CF_Yield ÷ Safety_Factor, "PASS", "FAIL") | - |

---

# SHEET 6: FIXED WING

## Purpose
Define wing geometry and aerodynamic coefficients for hybrid designs.

## Input Variables

| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Wing_Area | S | 2.0 | m² | Wing planform area |
| Wing_AR | AR | 8 | - | Aspect ratio |
| Wing_CLmax | C_L_max | 1.4 | - | Maximum lift coefficient |
| Wing_CLcruise | C_L_cr | 0.6 | - | Cruise lift coefficient |
| Wing_CD0 | C_D0 | 0.025 | - | Zero-lift drag coefficient |
| Wing_e | e | 0.85 | - | Oswald efficiency factor |
| UseWing | - | TRUE | - | Toggle: use wing lift in cruise |

## Calculated Variables

| Variable Name | Equation | Units |
|---------------|----------|-------|
| Wing_Span | = √(Wing_AR × Wing_Area) | m |
| Wing_Chord | = Wing_Area ÷ Wing_Span | m |

---

# SHEET 7: MISSION PROFILE

## Purpose
Define the mission parameters including payload, distances, and turns.

## Input Variables

| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Payload_Mass | m_pay | 49.9 | kg | Test payload (110 lb) |
| Mission_Distance | D_total | 9260 | m | Total course (5 nmi) |
| Distance_Loaded | D_loaded | 7408 | m | Loaded segment (4 nmi) |
| Distance_Unloaded | D_unloaded | 1852 | m | Unloaded segment (1 nmi) |
| Mission_Altitude | h_cruise | 107 | m | Cruise altitude (350 ft) |
| Turns_180 | N_180 | 0 | - | Number of U-turns |
| Turns_90 | N_90 | 0 | - | Number of 90° turns |
| Turns_45 | N_45 | 0 | - | Number of 45° turns |
| Mission_Speed | V_cruise | 25 | m/s | Target cruise speed |
| Mission_BankAngle | φ_max | 30 | degrees | Max bank angle in turns |
| Hover_Takeoff | t_TO | 30 | s | Hover time at takeoff |
| Hover_Landing | t_land | 30 | s | Hover time at landing |
| Transition_Time | t_trans | 10 | s | Hover-to-cruise transition |

## Calculated Variables

| Variable Name | Equation | Units |
|---------------|----------|-------|
| Total_Mass_Loaded | = Drone_EmptyMass + Payload_Mass | kg |
| Total_Weight_Loaded | = Total_Mass_Loaded × Gravity | N |
| Total_Mass_Unloaded | = Drone_EmptyMass | kg |
| Total_Weight_Unloaded | = Total_Mass_Unloaded × Gravity | N |

---

# SHEET 8: WEATHER

## Purpose
Define environmental conditions that affect air density and performance.

## Input Variables

| Variable Name | Symbol | Value | Units | Description |
|---------------|--------|-------|-------|-------------|
| Wind_Head | V_head | 0 | m/s | Headwind component |
| Wind_Tail | V_tail | 0 | m/s | Tailwind component |
| Wind_Cross | V_cross | 0 | m/s | Crosswind component |
| Temp_C | T | 15 | °C | Ambient temperature |
| Pressure | P_atm | 101325 | Pa | Barometric pressure |
| Gust_Velocity | V_gust | 5 | m/s | Design gust speed |

## Calculated Variables

| Variable Name | Equation | Units |
|---------------|----------|-------|
| AirDensity_Actual | = Pressure ÷ (287.05 × (Temp_C + 273.15)) | kg/m³ |
| Wind_Net | = MAX(0, Wind_Head - Wind_Tail) | m/s |
| Density_Ratio | = AirDensity_Actual ÷ AirDensity_SL | - |
| Groundspeed | = Mission_Speed + Wind_Tail - Wind_Head | m/s |

---

# SHEET 9: HOVER PERFORMANCE

## Purpose
Calculate all hover-related performance: thrust, power, current, time.

## Calculated Variables

### Thrust Requirements
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Thrust_Required_Total | = Total_Weight_Loaded × Safety_Factor | N |
| Thrust_Per_Motor | = Thrust_Required_Total ÷ Motor_Count | N |

### RPM Calculation
| Variable Name | Equation | Units |
|---------------|----------|-------|
| RPM_Required | = 60 × √(Thrust_Per_Motor ÷ (Prop_CT × AirDensity_Actual × Prop_Diameter_m⁴)) | RPM |
| RPM_Check | = IF(RPM_Required < Motor_MaxRPM, "PASS", "FAIL") | - |

### Torque Calculation
| Variable Name | Equation | Units |
|---------------|----------|-------|
| n_rps | = RPM_Required ÷ 60 | rev/s |
| Torque_Per_Motor | = Prop_CQ × AirDensity_Actual × n_rps² × Prop_Diameter_m⁵ | N·m |

### Power Calculation
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Power_Mech_Per_Motor | = Prop_CP × AirDensity_Actual × n_rps³ × Prop_Diameter_m⁵ | W |
| Power_Elec_Per_Motor | = Power_Mech_Per_Motor ÷ Motor_Efficiency | W |
| Power_Hover_Total | = Power_Elec_Per_Motor × Motor_Count | W |

### Current Calculation
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Current_Per_Motor | = Motor_I0 + (Torque_Per_Motor ÷ Motor_Kt) | A |
| Current_Hover_Total | = Current_Per_Motor × Motor_Count | A |
| Current_Check | = IF(Current_Hover_Total < Battery_MaxCurrent, "PASS", "FAIL") | - |

### Hover Time Available
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Hover_Time_Max | = (Battery_Capacity_Ah × 60) ÷ Current_Hover_Total | min |

---

# SHEET 10: CRUISE PERFORMANCE

## Purpose
Calculate fixed-wing cruise aerodynamics and power requirements.

## Calculated Variables (Loaded Segment)

### Lift Requirement
| Variable Name | Equation | Units |
|---------------|----------|-------|
| CL_Required_Loaded | = Total_Weight_Loaded ÷ (0.5 × AirDensity_Actual × Mission_Speed² × Wing_Area) | - |
| CL_Check | = IF(CL_Required_Loaded < Wing_CLmax, "PASS", "FAIL") | - |

### Drag Calculation
| Variable Name | Equation | Units |
|---------------|----------|-------|
| CDi_Loaded | = CL_Required_Loaded² ÷ (π × Wing_AR × Wing_e) | - |
| CD_Total_Loaded | = Wing_CD0 + CDi_Loaded | - |
| Drag_Loaded | = 0.5 × AirDensity_Actual × Mission_Speed² × Wing_Area × CD_Total_Loaded | N |

### Performance Metrics
| Variable Name | Equation | Units |
|---------------|----------|-------|
| LD_Ratio_Loaded | = CL_Required_Loaded ÷ CD_Total_Loaded | - |
| Stall_Speed | = √((2 × Total_Weight_Loaded) ÷ (AirDensity_Actual × Wing_Area × Wing_CLmax)) | m/s |

### Power & Current (Loaded)
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Thrust_Cruise_Loaded | = IF(UseWing, Drag_Loaded, Thrust_Required_Total) | N |
| Power_Cruise_Loaded | = (Thrust_Cruise_Loaded × Mission_Speed) ÷ (Prop_Efficiency × Motor_Efficiency) | W |
| Current_Cruise_Loaded | = Power_Cruise_Loaded ÷ Battery_Voltage | A |

## Calculated Variables (Unloaded Segment)

| Variable Name | Equation | Units |
|---------------|----------|-------|
| CL_Required_Unloaded | = Total_Weight_Unloaded ÷ (0.5 × AirDensity_Actual × Mission_Speed² × Wing_Area) | - |
| CDi_Unloaded | = CL_Required_Unloaded² ÷ (π × Wing_AR × Wing_e) | - |
| CD_Total_Unloaded | = Wing_CD0 + CDi_Unloaded | - |
| Drag_Unloaded | = 0.5 × AirDensity_Actual × Mission_Speed² × Wing_Area × CD_Total_Unloaded | N |
| Power_Cruise_Unloaded | = (Drag_Unloaded × Mission_Speed) ÷ (Prop_Efficiency × Motor_Efficiency) | W |

### Optimal Speeds
| Variable Name | Equation | Units |
|---------------|----------|-------|
| V_MinPower | = √((2 × Total_Weight_Loaded) ÷ (AirDensity_Actual × Wing_Area × √(3 × Wing_CD0 × π × Wing_AR × Wing_e))) | m/s |
| V_MaxRange | = V_MinPower × √(√3) | m/s |

---

# SHEET 11: TURN ANALYSIS

## Purpose
Calculate energy cost of turns and optimal turn strategy.

## Calculated Variables

### Turn Geometry
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Turn_Radius | = Mission_Speed² ÷ (Gravity × TAN(RADIANS(Mission_BankAngle))) | m |
| Load_Factor | = 1 ÷ COS(RADIANS(Mission_BankAngle)) | - |

### Arc Lengths
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Arc_180 | = Turn_Radius × π | m |
| Arc_90 | = Turn_Radius × π ÷ 2 | m |
| Arc_45 | = Turn_Radius × π ÷ 4 | m |

### Turn Times
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Time_Turn_180 | = Arc_180 ÷ Mission_Speed | s |
| Time_Turn_90 | = Arc_90 ÷ Mission_Speed | s |

### Turn Aerodynamics
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Lift_In_Turn | = Total_Weight_Loaded × Load_Factor | N |
| CL_In_Turn | = Lift_In_Turn ÷ (0.5 × AirDensity_Actual × Mission_Speed² × Wing_Area) | - |
| CDi_In_Turn | = CL_In_Turn² ÷ (π × Wing_AR × Wing_e) | - |
| Drag_In_Turn | = 0.5 × AirDensity_Actual × Mission_Speed² × Wing_Area × (Wing_CD0 + CDi_In_Turn) | N |
| Power_In_Turn | = (Drag_In_Turn × Mission_Speed) ÷ (Prop_Efficiency × Motor_Efficiency) | W |

### Turn Energy
| Variable Name | Equation | Units |
|---------------|----------|-------|
| Energy_Turn_180 | = (Power_In_Turn × Time_Turn_180) ÷ 3600 | Wh |
| Energy_Turn_90 | = (Power_In_Turn × Time_Turn_90) ÷ 3600 | Wh |
| Energy_Turn_45 | = Energy_Turn_90 ÷ 2 | Wh |
| Energy_Turns_Total | = (Turns_180 × Energy_Turn_180) + (Turns_90 × Energy_Turn_90) + (Turns_45 × Energy_Turn_45) | Wh |

---

# SHEET 12: ENERGY BUDGET

## Purpose
Complete mission energy breakdown and battery adequacy check.

## Time Calculations

| Variable Name | Equation | Units |
|---------------|----------|-------|
| Time_Cruise_Loaded | = Distance_Loaded ÷ Groundspeed | s |
| Time_Cruise_Unloaded | = Distance_Unloaded ÷ Groundspeed | s |
| Time_Hover_Total | = Hover_Takeoff + Hover_Landing | s |
| Time_Mission_Total | = (Time_Cruise_Loaded + Time_Cruise_Unloaded + Time_Hover_Total + Transition_Time) ÷ 60 | min |
| Time_Check | = IF(Time_Mission_Total < Time_Limit, "PASS", "FAIL") | - |

## Energy Calculations

| Variable Name | Equation | Units |
|---------------|----------|-------|
| Energy_Hover | = (Power_Hover_Total × Time_Hover_Total) ÷ 3600 | Wh |
| Energy_Transition | = (Power_Hover_Total × Transition_Time) ÷ 3600 | Wh |
| Energy_Climb | = (Total_Weight_Loaded × Mission_Altitude) ÷ 3600 | Wh |
| Energy_Cruise_Loaded | = (Power_Cruise_Loaded × Time_Cruise_Loaded) ÷ 3600 | Wh |
| Energy_Cruise_Unloaded | = (Power_Cruise_Unloaded × Time_Cruise_Unloaded) ÷ 3600 | Wh |
| Energy_Subtotal | = Energy_Hover + Energy_Transition + Energy_Climb + Energy_Cruise_Loaded + Energy_Cruise_Unloaded + Energy_Turns_Total | Wh |
| Energy_With_Reserve | = Energy_Subtotal × Safety_Factor | Wh |

## Battery Check

| Variable Name | Equation | Units |
|---------------|----------|-------|
| Energy_Available | = Battery_Usable_Wh | Wh |
| Energy_Margin_Pct | = ((Energy_Available - Energy_With_Reserve) ÷ Energy_With_Reserve) × 100 | % |
| Energy_Check | = IF(Energy_Available > Energy_With_Reserve, "PASS", "FAIL") | - |

---

# SHEET 13: PERFORMANCE SUMMARY

## Purpose
Dashboard showing all key metrics and pass/fail status.

## Key Performance Indicators

| Metric | Equation | Target | Status Equation |
|--------|----------|--------|-----------------|
| Payload_Ratio | = Payload_Mass ÷ Drone_EmptyMass | ≥ 4:1 | = IF(Payload_Ratio ≥ 4, "COMPETITIVE", IF(Payload_Ratio ≥ 2, "QUALIFYING", "FAIL")) |
| Drone_Weight_Check | = Drone_EmptyMass | ≤ 24.95 kg | = IF(Drone_EmptyMass ≤ 24.95, "PASS", "FAIL") |
| Mission_Time | = Time_Mission_Total | < 30 min | = Time_Check |
| Energy_Status | = Energy_Margin_Pct | > 0% | = Energy_Check |
| Hover_Disk_Loading | = Thrust_Required_Total ÷ (Motor_Count × Prop_Area) | < 500 N/m² | = IF(Hover_Disk_Loading < 500, "GOOD", "HIGH") |
| Cruise_LD_Ratio | = LD_Ratio_Loaded | > 8 | = IF(LD_Ratio_Loaded > 8, "GOOD", "LOW") |
| Stall_Margin | = Mission_Speed ÷ Stall_Speed | > 1.3 | = IF(Stall_Margin > 1.3, "SAFE", "RISKY") |
| RPM_Status | - | - | = RPM_Check |
| Current_Status | - | - | = Current_Check |
| Structure_Status | - | - | = Stress_Check |
| Safety_Factor_Used | = Safety_Factor | - | Display current SF value |

## Summary Table

| Check | Status | Notes |
|-------|--------|-------|
| Weight Under Limit | = Drone_Weight_Check | Must be ≤ 55 lb |
| Payload Ratio | = Payload status | Target 4:1 for competitive |
| Mission Time | = Time_Check | Must complete < 30 min |
| Energy Sufficient | = Energy_Check | With reserves |
| Motors Can Deliver | = RPM_Check | RPM within limits |
| Current Sustainable | = Current_Check | Below battery max |
| Structure Safe | = Stress_Check | SF ≥ 2.0 |

---

# CROSS-SHEET REFERENCE SUMMARY

When building formulas, reference variables from other sheets using:
```
=SheetName!VariableName
```

Or better, use **Named Ranges** (recommended):
1. Select the cell
2. Type the variable name in the Name Box
3. Reference by name anywhere: `=Safety_Thrust`

## Key Dependencies

```
Constants & Safety → ALL SHEETS (safety factors, constants)
Propeller → Hover Performance, Cruise Performance (coefficients)
Motors → Hover Performance, Energy Budget (power limits)
Battery → ALL power/energy calculations (voltage, capacity)
Airframe → Mission Profile (empty mass)
Fixed Wing → Cruise Performance, Turn Analysis (aero coefficients)
Mission Profile → Energy Budget, Performance Summary (distances, times)
Weather → Hover Performance, Cruise Performance (air density)
Hover Performance → Energy Budget (hover power)
Cruise Performance → Energy Budget (cruise power)
Turn Analysis → Energy Budget (turn energy)
Energy Budget → Performance Summary (energy status)
```

---

# SENSITIVITY ANALYSIS SETUP

On the Constants & Safety sheet, add scenario rows for the single Safety_Factor:

| Scenario | Safety_Factor | Description |
|----------|---------------|-------------|
| Conservative | 1.5 | +50% margin on everything |
| Baseline | 1.2 | +20% margin (recommended) |
| Aggressive | 1.1 | +10% margin (risky) |
| Minimal | 1.0 | No margin (theoretical only) |
| **SELECTED** | (use dropdown) | Active value used in all calculations |

Use Data Validation dropdown to switch between scenarios and see how the single safety factor affects:
- Required thrust and hover power
- Structural sizing and stress limits
- Energy reserves and battery requirements
- Overall payload-to-weight ratio

---

# NEXT STEPS

1. Create the workbook with all 13 sheets
2. Enter input values on sheets 1-8
3. Build formulas on sheets 9-13 using the equations above
4. Create Named Ranges for all key variables
5. Add conditional formatting (green/red) for pass/fail checks
6. Test with known values to verify calculations
7. Iterate on design parameters to maximize payload ratio