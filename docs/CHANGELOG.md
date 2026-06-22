# Changelog

All notable changes to 1DSIM are documented here.

---

## [1.0.0] — 2026-06-22

### 🚀 Initial Release

#### Core C++ Solver (`src/core/`)
- `ThermalNode` struct with solid and fluid domain support
- `ThermalLink` struct supporting 5 heat transfer modes
- `GetFluidProperties()` — temperature-dependent ρ(T) and cₚ(T) for all media
- `GetNodeCapacityAndDeriv()` — dynamic thermal capacity for fluid and solid nodes
- `GetLinkFlowRateAndDeriv()` — unified advective transport for LINK_FLOW and LINK_FAN

#### Heat Transfer Modes
- **LINK_CONDUCTION (0)** — Fourier conduction G·ΔT
- **LINK_CONVECTION (1)** — Newton convection hA·ΔT
- **LINK_RADIATION (2)** — Stefan-Boltzmann σεA·(T_A⁴ − T_B⁴)
- **LINK_FLOW (3)** — Prescribed volumetric flow [L/min] advection
- **LINK_FAN (4)** — Analytical fan-system quadratic intersection

#### Fluid Media
- **Water** — cubic polynomial ρ(T), quadratic cₚ(T)
- **Glycol 50/50** — linear ρ(T), linear cₚ(T)
- **Engine Oil** — linear ρ(T), linear cₚ(T)
- **Air** — ideal gas ρ(T) = P/(RT), linear cₚ(T)
- **Mixture** — water-glycol blend with user-defined glycol fraction (0–100%)
- **Custom** — user-specified polynomial coefficients for ρ(T) and cₚ(T)

#### Numerical Solvers
- Explicit Euler (1st order)
- Runge-Kutta 4 (4th order, default)
- Implicit Backward Euler (Newton iteration, unconditionally stable)
- Steady-State Newton solver (converges in 2–5 iterations)

#### Python Layer (`src/wrapper/`, `src/gui/`)
- `ThermalSolverWrapper` — ctypes bindings for all DLL exports
- `ThermalSystem` — high-level Python API with Celsius/Kelvin helpers
- `set_node_fluid_params()` — Mixture/Custom fluid property injection
- `server.py` — HTTP REST API (`/api/init`, `/api/step`, `/api/solve_steady`)

#### Web Interface (`src/gui/web/`)
- Drag-and-drop schematic canvas with orthogonal link routing
- Live temperature colour map (blue → red gradient)
- Animated flow chevrons with direction from velocity sign
- **Fan / Pump** palette item (violet, impeller icon)
- **Fan Matcher** tab — Chart.js fan curve vs. system resistance with operating point
- **Attribute Sheet** — live editing for all node/link parameters
- Fluid domain properties: medium dropdown, mixture slider, custom polynomial inputs
- 4 built-in presets: Vehicle Cooling, CPU Cooler, Battery Pack, Window Insulation
- Dynamic variable sliders per preset
- CSV export of full time-history

#### Desktop Application (`src/desktop/`)
- Native Windows app with Dear ImGui + ImPlot + DirectX 11
- Schematic editor with grid snap, zoom/pan, node drag
- Attribute Sheet for all component types
- **ImPlot Fan Matcher** panel with operating point diamond marker
- `.gtm` JSON project file save/load (backward compatible, `fan_area` defaults to 0.005)
- 60 fps render loop with animated directional flow chevrons

#### Tests (`tests/`)
- 9 physics verification tests, all passing at **0.000000% relative error**
- `test_fluid_capacities` — 4 media capacity validation
- `test_fluid_advection` — upstream enthalpy transport
- `test_mixture_fluid` — linear interpolation at 40% glycol
- `test_custom_fluid` — polynomial ρ(T), cₚ(T) evaluation
- `test_fan_link` — quadratic root operating velocity → advective heat rate
- `test_transient_cooling` — RK4 vs analytical exponential decay
- `test_steady_state` — Newton iteration convergence
- `test_stiff_system_implicit` — Implicit Euler stiff system stability
- `test_temperature_dependence` — Nonlinear C(T) during transient

#### Repository
- `README.md` — full project documentation
- `docs/ARCHITECTURE.md` — system architecture and data flow
- `docs/PHYSICS.md` — complete physics reference with equations
- `docs/CHANGELOG.md` — this file
- `.gitignore` — excludes compiled binaries and cache
- Thirdparty vendored: Dear ImGui, ImPlot, nlohmann/json
