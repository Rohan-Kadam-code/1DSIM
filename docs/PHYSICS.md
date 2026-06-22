# Physics Reference

## Heat Transfer Modes

### 1. Conduction (Link Type 0)

Fourier's law of heat conduction:

```
Q = G · (T_A − T_B)     [W]
```

where `G` is the thermal conductance [W/K].

### 2. Convection (Link Type 1)

Newton's law of cooling:

```
Q = hA · (T_A − T_B)    [W]
```

where `hA` is the convection coefficient × area [W/K].

### 3. Radiation (Link Type 2)

Stefan-Boltzmann radiation:

```
Q = σ·ε·A · (T_A⁴ − T_B⁴)    [W]
```

where `p1 = σ·ε·A` and temperatures are in Kelvin.

### 4. Fluid Advection (Link Type 3)

Enthalpy transport with upstream-biased scheme:

```
Q_net = ṁ·cₚ(T_up) · T_up − ṁ·cₚ(T_down) · T_down
```

The volumetric flow rate [L/min] is prescribed directly as `p1`. The mass flow is computed at the upstream node temperature using `ρ(T_up)·cₚ(T_up)`.

### 5. Fan / Pump (Link Type 4)

#### Fan Curve Model

The fan is modelled as a quadratic pressure-velocity curve:

```
P_fan(v) = P_max − B·v²     where B = P_max / v_max²
```

Parameters:
- `P_max` = shut-off pressure [Pa] (at v = 0)
- `v_max` = free-delivery velocity [m/s] (at P = 0)

#### System Resistance Model

```
P_sys(v) = K·v² + R·v
```

Parameters:
- `K` = quadratic resistance coefficient [Pa/(m/s)²]
- `R` = linear resistance coefficient [Pa/(m/s)]

#### Operating Point — Exact Analytical Solution

Setting P_fan = P_sys:

```
P_max − B·v² = K·v² + R·v
(K + B)·v² + R·v − P_max = 0
```

Solving the quadratic (taking the positive root):

```
v_oper = (−R + √(R² + 4·(K + B)·P_max)) / (2·(K + B))
```

Volumetric flow rate:

```
Q_vol = v_oper · A · 60000    [L/min]
```

where `A` is the cross-sectional flow area [m²].

---

## Fluid Media

### Built-in Properties

All properties are polynomial functions of temperature T [°C]:

| Medium | ρ(T) [kg/m³] | cₚ(T) [J/(kg·K)] |
|--------|-------------|------------------|
| Water | 1000 − 0.0178T − 0.00557T² + 0.000027T³ | 4184 − 0.09T + 0.006T² |
| Glycol 50/50 | 1060 − 0.65T | 3300 + 3.5T |
| Engine Oil | 890 − 0.60T | 1800 + 3.0T |
| Air | 353.18 / (T + 273.15) | 1005 + 0.05T |

### Mixture (Water-Glycol Blend)

Linear interpolation by glycol fraction r ∈ [0, 1]:

```
ρ_mix(T) = (1 − r)·ρ_water(T) + r·ρ_glycol(T)
cₚ_mix(T) = (1 − r)·cₚ_water(T) + r·cₚ_glycol(T)
```

### Custom Fluid

User-specified quadratic polynomials:

```
ρ(T) = a₀ + a₁·T + a₂·T²    [kg/m³]
cₚ(T) = b₀ + b₁·T + b₂·T²   [J/(kg·K)]
```

---

## Thermal Capacity

### Solid Node

```
C = c_a0 + c_a1·T + c_a2·T²    [J/K]
```

### Fluid Node

```
C = ρ(T) · V · cₚ(T)            [J/K]
```

where V is the fluid volume in litres (converted to m³ internally: V_m³ = V_L × 10⁻³).

---

## Numerical Solvers

### Explicit Euler

```
T(t + dt) = T(t) + dt · Q_net(T(t)) / C(T(t))
```

First-order accurate. Fast but conditionally stable (requires small dt).

### Runge-Kutta 4 (RK4)

Standard 4-stage Runge-Kutta. Fourth-order accurate. Default solver.

### Implicit Backward Euler

Newton iteration on the residual:

```
R(T*) = C(T*)·(T* − T) / dt − Q_net(T*) = 0
```

Unconditionally stable. Suitable for stiff systems (e.g. large conductances with small capacities).

### Steady-State Solver

Solves `Q_net(T*) = 0` via Newton iteration:

```
T_new = T_old − Q_net(T_old) / (∂Q_net/∂T evaluated at T_old)
```

Converges in O(2–5) iterations for typical linear networks.
