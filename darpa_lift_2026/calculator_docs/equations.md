I've created a comprehensive physics and engineering equation reference for your drone design. This covers:

**Core Propulsion:**
- Complete propeller aerodynamics (momentum theory, blade element theory)
- Motor torque-speed relationships and efficiency
- Battery capacity, discharge rates, and energy calculations

**Structural Analysis:**
- Bending moments, shear forces, and deflection for cantilever beams
- Stress analysis and section properties for tubes/rods
- Buckling calculations for compression members
- Material properties and specific strength comparisons

**Dynamic Effects:**
- Load factors for maneuvers and gusts
- Vibration analysis and resonance avoidance
- Fatigue considerations

**System Integration:**
- Design algorithm framework showing calculation sequence
- Optimization approach to maximize payload-to-weight ratio
- All equations needed to verify structural integrity with safety factors

The key insight: **carbon fiber tubes** will be your friend here - they have 3-4x better strength-to-weight ratio than aluminum. You'll want to model your arms as cantilever beams with combined point loads (motors/props) and distributed loads (payload fraction per arm), then iterate on tube dimensions to minimize weight while maintaining SF ≥ 2.0.

Want me to create a computational tool to run these calculations, or help you work through a specific design configuration?


# Drone Design - Physics & Engineering Equations

## 1. Propeller Aerodynamics

### Thrust Equations

**Momentum Theory (Ideal Thrust):**
```
T = ρ × A × v_e × (v_e + 2v_∞)

Where:
T = Thrust (N or lbf)
ρ = Air density (kg/m³ or slug/ft³)
A = Propeller disc area = π × R² (m² or ft²)
R = Propeller radius (m or ft)
v_e = Induced velocity at propeller (m/s or ft/s)
v_∞ = Freestream velocity (m/s or ft/s) [0 for hover]
```

**Hover Thrust (Simplified):**
```
T = C_T × ρ × n² × D⁴

Where:
C_T = Thrust coefficient (dimensionless, ~0.01-0.15)
n = Rotational speed (rev/s)
D = Propeller diameter (m or ft)
```

**RPM-Based Thrust:**
```
T = C_T × ρ × (RPM/60)² × D⁴

RPM = Revolutions per minute
```

### Power Requirements

**Ideal Power (Momentum Theory):**
```
P_ideal = T × v_i

Where:
v_i = Induced velocity = √(T / (2ρA))
```

**Actual Power:**
```
P = C_P × ρ × n³ × D⁵

Where:
C_P = Power coefficient (dimensionless)
```

**Figure of Merit (Propeller Efficiency in Hover):**
```
FM = P_ideal / P_actual = T^(3/2) / (√(2ρA) × P)

Typical values: 0.6-0.8 for good propellers
```

**Power with Forward Flight:**
```
P_total = P_induced + P_profile + P_parasite

P_induced = T × v_i
P_profile = (σ × C_d0 × ρ × A × (ΩR)³) / 8
P_parasite = D_parasite × v_∞

Where:
σ = Solidity ratio (blade area / disc area)
C_d0 = Profile drag coefficient
Ω = Angular velocity (rad/s)
D_parasite = Parasitic drag
```

### Torque

```
Q = P / (2πn) = P / ω

Where:
Q = Torque (N⋅m or lb⋅ft)
ω = Angular velocity (rad/s) = 2πn
```

**Torque Coefficient:**
```
Q = C_Q × ρ × n² × D⁵

Where:
C_Q = Torque coefficient ≈ C_P / (2π)
```

### Propeller Advance Ratio

```
J = v_∞ / (n × D)

Where:
J = Advance ratio (dimensionless)
Used to look up C_T, C_P, C_Q from propeller performance data
```

### Blade Element Theory (More Accurate)

```
dT = (1/2) × ρ × W² × c × (C_l × cos(φ) - C_d × sin(φ)) × dr
dQ = (1/2) × ρ × W² × c × r × (C_l × sin(φ) + C_d × cos(φ)) × dr

Where:
W = Resultant velocity at blade element
c = Chord length at radius r
C_l, C_d = Lift and drag coefficients
φ = Inflow angle
dr = Differential radius element

Integrate from root to tip for total thrust/torque
```

---

## 2. Motor Performance

### Motor Power

```
P_mech = T × Q × ω / η_motor

Where:
P_mech = Mechanical power output (W)
η_motor = Motor efficiency (0.7-0.9 typical)
```

**Electrical Power:**
```
P_elec = V × I

Where:
V = Voltage (V)
I = Current (A)
```

**Motor Efficiency:**
```
η_motor = P_mech / P_elec
```

### Motor Torque-Speed Relationship (DC Brushless)

```
ω = ω_0 - (R_m / K_t²) × Q

Where:
ω_0 = No-load speed (rad/s) = V / K_v
K_v = Motor velocity constant (RPM/V)
K_t = Torque constant (N⋅m/A)
R_m = Motor resistance (Ω)
```

**KV to Angular Velocity:**
```
ω_0 = (K_v × V × 2π) / 60

RPM = K_v × V
```

**Torque-Current Relationship:**
```
Q = K_t × I

K_t = 60 / (2π × K_v)  [in SI units]
```

### Motor Current Draw

```
I = I_0 + Q / K_t

Where:
I_0 = No-load current (A)
```

**Total Current (All Motors):**
```
I_total = N_motors × I_per_motor
```

### Motor Heating/Thermal Limits

```
P_loss = I² × R_m + P_iron + P_windage

Temp_rise = P_loss × R_thermal

Where:
R_thermal = Thermal resistance (°C/W)
Max continuous operation limited by temperature
```

---

## 3. Battery Specifications

### Battery Capacity

```
E = V_nominal × C

Where:
E = Energy capacity (Wh)
C = Capacity (Ah)
```

**C-Rating (Discharge Rate):**
```
I_max = C × C_rating

Where:
C_rating = Maximum continuous discharge rate
Example: 20C rating on 10Ah battery = 200A max
```

### Flight Time Estimation

```
t_flight = (C × 60) / I_avg

Where:
t_flight = Flight time (minutes)
I_avg = Average current draw (A)
```

**Energy-Based Flight Time:**
```
t_flight = E / P_avg

Where:
P_avg = Average power consumption (W)
```

### Battery Mass

```
m_battery = E / e_specific

Where:
e_specific = Specific energy (Wh/kg)
Typical Li-Po: 150-250 Wh/kg
```

### Voltage Sag Under Load

```
V_load = V_nominal - I × R_internal

Where:
R_internal = Internal resistance (Ω)
```

**Cell Voltage vs SOC (State of Charge):**
```
V_cell = f(SOC)

Typical Li-Po:
Fully charged: 4.2V/cell
Nominal: 3.7V/cell
Discharged: 3.0V/cell (cutoff)
```

---

## 4. Structural Analysis - Arms/Booms

### Bending Moment

**Cantilever Beam with Point Load:**
```
M(x) = -P × (L - x)

Where:
M(x) = Bending moment at position x (N⋅m or lb⋅ft)
P = Point load (N or lbf)
L = Beam length (m or ft)
x = Distance from fixed end

M_max = P × L (at fixed end)
```

**Cantilever Beam with Distributed Load:**
```
M(x) = -w × (L - x)² / 2

Where:
w = Distributed load per unit length (N/m or lb/ft)

M_max = w × L² / 2
```

**Combined Loading (Point + Distributed):**
```
M(x) = -P × (L - x) - w × (L - x)² / 2
```

### Bending Stress

**Flexural Stress (Beam Bending):**
```
σ = M × y / I

Where:
σ = Bending stress (Pa or psi)
y = Distance from neutral axis to outer fiber (m or in)
I = Second moment of area (m⁴ or in⁴)
```

**Maximum Bending Stress:**
```
σ_max = M_max × c / I = M_max / S

Where:
c = Distance to extreme fiber
S = Section modulus = I / c
```

### Section Properties

**Circular Tube:**
```
I = π × (D_o⁴ - D_i⁴) / 64

Where:
D_o = Outer diameter
D_i = Inner diameter

S = I / (D_o / 2)
A = π × (D_o² - D_i²) / 4
```

**Rectangular Tube:**
```
I = (b × h³ - b_i × h_i³) / 12

Where:
b, h = Outer width and height
b_i, h_i = Inner width and height

A = b × h - b_i × h_i
```

**Solid Circular Rod:**
```
I = π × D⁴ / 64
S = π × D³ / 32
A = π × D² / 4
```

### Shear Force and Shear Stress

**Shear Force (Cantilever):**
```
V(x) = -P - w × (L - x)

Where:
V = Shear force (N or lbf)
```

**Shear Stress:**
```
τ = V × Q / (I × t)

Where:
τ = Shear stress (Pa or psi)
Q = First moment of area
t = Thickness at point of interest

For rectangular sections:
τ_max = 3V / (2A)
```

### Deflection

**Maximum Deflection (Cantilever with Point Load at End):**
```
δ_max = P × L³ / (3 × E × I)

Where:
δ = Deflection (m or in)
E = Young's modulus (Pa or psi)
```

**Maximum Deflection (Cantilever with Distributed Load):**
```
δ_max = w × L⁴ / (8 × E × I)
```

**Combined Loading:**
```
δ_max = (P × L³) / (3 × E × I) + (w × L⁴) / (8 × E × I)
```

**Deflection Equation (General):**
```
EI × d⁴y/dx⁴ = w(x)

Solve with boundary conditions for specific loading
```

### Buckling (Column Stability)

**Euler Buckling Load:**
```
P_cr = (π² × E × I) / (K × L)²

Where:
P_cr = Critical buckling load (N or lbf)
K = Effective length factor
  K = 1 for pinned-pinned
  K = 2 for fixed-free (cantilever)
  K = 0.5 for fixed-fixed
```

**Slenderness Ratio:**
```
λ = K × L / r

Where:
r = Radius of gyration = √(I / A)
```

### Combined Stress (von Mises)

```
σ_vm = √(σ_x² - σ_x × σ_y + σ_y² + 3τ_xy²)

For uniaxial bending + torsion:
σ_vm = √(σ_bending² + 3τ_torsion²)
```

### Safety Factor

```
SF = σ_yield / σ_max

Or for buckling:
SF = P_cr / P_applied

Typical SF for aerospace: 1.5-2.0
For this competition: Recommend 2.0-3.0
```

---

## 5. Material Properties

### Key Material Parameters

```
σ_yield = Yield strength (Pa or psi)
σ_ultimate = Ultimate tensile strength (Pa or psi)
E = Young's modulus (Pa or psi)
G = Shear modulus (Pa or psi)
ρ_material = Density (kg/m³ or lb/ft³)
ν = Poisson's ratio (dimensionless)
```

**Specific Strength (Strength-to-Weight Ratio):**
```
σ_specific = σ_yield / ρ_material

Higher is better for aerospace
```

**Typical Materials:**

| Material | E (GPa) | σ_yield (MPa) | ρ (kg/m³) | σ_specific |
|----------|---------|---------------|-----------|------------|
| Aluminum 6061-T6 | 69 | 276 | 2700 | 102 |
| Carbon Fiber (tube) | 150 | 600 | 1600 | 375 |
| Titanium Ti-6Al-4V | 114 | 880 | 4430 | 199 |
| Steel 4130 | 200 | 460 | 7850 | 59 |

---

## 6. Dynamic Loading & Vibration

### Dynamic Load Factor

```
P_dynamic = P_static × n_z

Where:
n_z = Load factor (g's)
For maneuvers: 1.5-3.0g typical
For gusts/turbulence: Add 0.5-1.0g
```

**Gust Load:**
```
Δn = (ρ × v_gust × a × C_L × V) / (2 × W)

Where:
v_gust = Gust velocity (m/s)
a = Lift curve slope
V = Flight velocity
W = Weight
```

### Natural Frequency (Cantilever Beam)

```
f_n = (λ_n² / (2π)) × √(E × I / (ρ_material × A × L⁴))

Where:
f_n = Natural frequency (Hz)
λ_1 = 1.875 (first mode)
λ_2 = 4.694 (second mode)
```

**Forced Vibration from Motors:**
```
f_excitation = (RPM / 60) × N_blades

Avoid resonance: f_excitation ≠ f_n
Recommended: f_excitation < 0.5 × f_n or > 2 × f_n
```

### Fatigue Life

```
N_f = C / (Δσ^m)

Where:
N_f = Cycles to failure
Δσ = Stress range
C, m = Material constants (S-N curve)
```

---

## 7. Thrust-to-Weight Requirements

### Total Thrust Required

```
T_total = (m_drone + m_payload) × g × (1 + margin)

Where:
g = 9.81 m/s² or 32.2 ft/s²
margin = Acceleration capability (0.2-0.5 typical)
```

**Per-Motor Thrust:**
```
T_motor = T_total / N_motors
```

### Disk Loading

```
DL = T_total / A_total

Where:
A_total = Total disc area of all propellers
Lower DL = More efficient (larger props)
```

### Power Loading

```
PL = T_total / P_total

Higher PL = More efficient
Units: N/W or lbf/hp
```

---

## 8. Flight Performance

### Horizontal Velocity (Cruise)

```
v_cruise = √(T_horizontal / (0.5 × ρ × A_frontal × C_D))

Where:
A_frontal = Frontal area
C_D = Drag coefficient
```

**Tilt for Forward Flight:**
```
θ = arctan(D / T)

Where:
θ = Tilt angle
D = Drag force
```

### Rate of Climb

```
RC = (P_available - P_required) / W

Where:
RC = Rate of climb (m/s or ft/min)
P_available = Total motor power
P_required = Power to hover
W = Total weight
```

### Endurance

```
E_max when: P = minimum

Occurs at velocity: v_E = √((2W) / (ρ × A_frontal × C_D))
```

### Range

```
R = (η × E_battery) / (D × g)

Simplified for electric:
R ≈ v_cruise × t_flight
```

---

## 9. System-Level Performance Metrics

### Payload-to-Weight Ratio

```
PWR = m_payload / m_drone

Target: PWR ≥ 4:1 for competitive design
```

### Specific Endurance

```
SE = t_flight / m_total

Units: min/kg
```

### Mission Score (Custom Metric)

```
Score = PWR × (t_flight / t_required) × (1 - penalty_factors)

Where:
t_required = 30 minutes
penalty_factors = structural risk, cost, complexity
```

---

## 10. Design Algorithm Framework

### Input Parameters
- Number of motors (N_motors)
- Propeller diameter (D)
- Motor KV rating
- Battery voltage (V) and capacity (C)
- Arm length (L)
- Arm cross-section dimensions
- Material properties
- Target payload (m_payload)

### Calculation Sequence

1. **Calculate required thrust per motor:**
   - T_motor = (m_drone + m_payload) × g × margin / N_motors

2. **Select propeller and estimate RPM:**
   - RPM = f(T_motor, D, C_T) using thrust equation

3. **Calculate required motor torque:**
   - Q = C_Q × ρ × (RPM/60)² × D⁵

4. **Verify motor can deliver torque/RPM:**
   - Check motor specs: ω_max, Q_max, I_max

5. **Calculate power consumption:**
   - P_motor = Q × ω / η_motor
   - I = P / V

6. **Calculate flight time:**
   - t_flight = (C × 60) / (N_motors × I)

7. **Structural verification:**
   - Calculate M_max on arms
   - Calculate σ_max and compare to σ_yield / SF
   - Calculate deflection and check limits
   - Check buckling if compressive loads

8. **Score calculation:**
   - PWR = m_payload / m_drone
   - Mission_score = f(PWR, t_flight, SF, cost)

### Output Metrics
- Payload-to-weight ratio
- Estimated flight time
- Structural safety factors
- Power efficiency
- Total system weight breakdown
- Risk assessment

---

## 11. Iterative Optimization

```
Objective function:
Maximize: PWR = m_payload / m_drone

Subject to constraints:
- t_flight ≥ 30 minutes
- SF_structural ≥ 2.0
- T_available ≥ T_required
- δ_max ≤ δ_allowable
- m_drone ≤ 55 lbs
- All component ratings not exceeded
```

**Design Variables:**
- N_motors (4, 6, 8, 12, etc.)
- D_propeller
- L_arm
- V_battery (number of cells)
- Arm geometry (tube dimensions)
- Material selection

**Use numerical optimization or parametric sweep to find optimal design.**