# 1DSIM — 1D Thermal Network Simulator

<div align="center">

![GT-Thermal 1D](https://img.shields.io/badge/Physics-Thermal%20Network-blue?style=for-the-badge)
![C++](https://img.shields.io/badge/Core-C%2B%2B17-00599C?style=for-the-badge&logo=cplusplus)
![Python](https://img.shields.io/badge/API-Python%203.11-3776AB?style=for-the-badge&logo=python)
![ImGui](https://img.shields.io/badge/Desktop-Dear%20ImGui-blueviolet?style=for-the-badge)
![Tests](https://img.shields.io/badge/Tests-9%20Passing%20%7C%200%25%20Error-brightgreen?style=for-the-badge)

**A high-performance 1D thermal network simulator with a compiled C++ solver core, Python REST API, interactive browser-based schematic editor, and a native Windows desktop CAD application — inspired by GT-ISE / GT-Suite.**

</div>

---

## Features

### 🔬 Physics Engine (C++ Core)

| Feature | Details |
|---------|---------|
| **Heat Transfer Modes** | Conduction · Convection · Radiation · Advective Fluid Flow |
| **Fan / Pump Link** | Exact analytical quadratic root — fan curve vs. system resistance |
| **Fluid Media** | Water · Glycol 50/50 · Engine Oil · Air · **Mixture** (0–100% glycol) · **Custom** |
| **Custom Fluid** | User-defined polynomial ρ(T) and cₚ(T) coefficients |
| **Solvers** | Explicit Euler · RK4 · Implicit Backward Euler (Newton iteration) |
| **Steady State** | Nonlinear Newton iteration with configurable tolerance |
| **T-dependent props** | All node capacities and link conductances support C(T), G(T) |

### 🌐 Web Interface

- **Drag-and-drop schematic canvas** — nodes, orthogonal links, live temperature colour map
- **Live transient simulation** — animated flow chevrons during run
- **Attribute Sheet** — click any element to edit all parameters live
- **Fan Matcher tab** — Chart.js plot of fan curve vs. system resistance with operating point
- **4 built-in presets** — Vehicle Cooling Loop · CPU Cooler · Li-Ion Battery Pack · Double-Pane Window
- **Dynamic Variable sliders** — adjust power, flow rate, ambient temperature during simulation
- **CSV export** — full time-history of all node temperatures

### 🖥️ Desktop Application

- Native Windows app using **Dear ImGui + ImPlot + DirectX 11**
- **Physical Component Library** — Place standard ICE cooling components directly on the canvas:
  - *Engine Block, Radiator, Radiator Fan, Water Pump, Thermostat, Coolant Hose, Oil Cooler, Heater Core, Expansion Tank, and Ambient Air*
  - Auto-compiles component/connection graphs into the flat solver representation
- Schematic editor with grid snap, zoom/pan, multi-select
- Full Attribute Sheet editing for all component types
- **ImPlot Fan Matcher** panel — fan & resistance curves with operating point marker
- Save/load `.gtm` JSON project files (backward compatible)
- Animated directional flow chevrons at 60 fps

---

## Project Structure

```
1DSIM/
├── src/
│   ├── core/               # C++ solver engine
│   │   ├── solver.h        # ThermalNode, ThermalLink, ThermalSystem API
│   │   ├── solver.cpp      # Physics: solvers, fluid properties, fan matching
│   │   └── bindings.cpp    # C-ABI DLL exports for Python ctypes
│   ├── wrapper/
│   │   └── thermal_solver.py   # Python ctypes wrapper
│   ├── gui/
│   │   ├── server.py           # Python HTTP REST server
│   │   └── web/
│   │       ├── index.html      # Browser schematic editor
│   │       ├── app.js          # Simulation logic, canvas rendering
│   │       └── style.css       # GT-Suite-inspired dark theme
│   └── desktop/
│       └── main.cpp            # Native ImGui + ImPlot application
├── tests/
│   ├── test_simulation.py      # Core solver tests (transient, steady, stiff)
│   └── test_media_simulation.py # Fluid media + fan link physics tests
├── thirdparty/
│   ├── imgui/                  # Dear ImGui (vendored)
│   ├── implot/                 # ImPlot (vendored)
│   └── json.hpp                # nlohmann/json single header
├── build.py                    # Build solver DLL (MSVC)
├── build_desktop.py            # Build thermal_sim.exe (MSVC + ImGui)
└── run.py                      # Launch web server shortcut
```

---

## Getting Started

### Prerequisites

| Tool | Version |
|------|---------|
| Python | 3.10+ |
| MSVC (Visual Studio Build Tools) | 2022 |
| pytest | `pip install pytest` |

### 1. Build the C++ Solver DLL

```powershell
python build.py
# Outputs: src/core/solver.dll
```

### 2. Run the Web Interface

```powershell
python run.py
# Opens http://localhost:8000 automatically
```

### 3. Build & Run the Desktop App

```powershell
python build_desktop.py
# Outputs: thermal_sim.exe
./thermal_sim.exe
```

### 4. Run the Test Suite

```powershell
pytest -v
# 9 tests, 0.000000% relative error on all physics checks
```

---

## Physics Reference

### Fan / Pump Operating Point

The fan curve is modelled as a quadratic:

```
P_fan(v) = P_max − B·v²     where B = P_max / v_max²
```

The system resistance:

```
P_sys(v) = K·v² + R·v
```

Setting P_fan = P_sys and solving analytically:

```
v_oper = (−R + √(R² + 4(K + B)·P_max)) / (2(K + B))
Q_vol  = v_oper × A × 60000   [L/min]
```

### Fluid Mixture Interpolation

```
ρ_mix(T)  = (1 − r)·ρ_water(T)  + r·ρ_glycol(T)
cₚ_mix(T) = (1 − r)·cₚ_water(T) + r·cₚ_glycol(T)
```

where `r` is the glycol volume fraction (0 = pure water, 1 = pure glycol).

### Custom Fluid Polynomials

```
ρ(T)  = a₀ + a₁·T + a₂·T²   [kg/m³]
cₚ(T) = b₀ + b₁·T + b₂·T²   [J/(kg·K)]
```

---

## Test Results

All 9 tests pass with **0.000000% relative error** against closed-form analytical solutions:

| Test | Validates |
|------|-----------|
| `test_fluid_capacities` | ρ(T)·V·cₚ(T) for Water, Glycol, Oil, Air |
| `test_fluid_advection` | Upstream enthalpy transport in closed loop |
| `test_mixture_fluid` | Linear interpolation at 40% glycol fraction |
| `test_custom_fluid` | Polynomial ρ(T), cₚ(T) evaluation |
| `test_fan_link` | Quadratic root v_oper → advective heat rate |
| `test_transient_cooling` | RK4 exponential temperature decay |
| `test_steady_state` | Newton iteration convergence |
| `test_stiff_system_implicit` | Implicit Euler stiff system stability |
| `test_temperature_dependence` | Nonlinear C(T) during transient |

---

## Built-in Presets

| Preset | Nodes | Links | Description |
|--------|-------|-------|-------------|
| Vehicle Cooling Loop | 5 | 5 | Engine → Jacket → Radiator → Ambient |
| CPU Cooler Assembly | 4 | 3 | Die → TIM → Heat Sink → Air |
| Li-Ion Battery Pack | 10 | 12 | 4-cell array with serpentine coolant channels |
| Double-Pane Window | 5 | 4 | Indoor → Glass → Argon gap → Glass → Outdoor |

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Solver core | C++17 · MSVC |
| Python binding | `ctypes` |
| Web server | Python `http.server` |
| Web UI | Vanilla HTML/CSS/JS · Chart.js |
| Desktop UI | Dear ImGui · ImPlot · DirectX 11 |
| Tests | pytest |
| JSON I/O | nlohmann/json |

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

<div align="center">
Built to demonstrate real-world 1D thermal simulation competency for automotive and motorsport engineering applications.
</div>
