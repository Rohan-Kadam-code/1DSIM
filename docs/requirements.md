# System and Functional Requirements Specification
## 1DSIM Thermal Simulator Requirements Database
*Owner: Rohan Kadam (Systems Architect) | Status: Approved*

---

## 1. System Integration Solvers (SYS)

### REQ-SYS-001: Runge-Kutta 4th Order (RK4) Solver
- **Description**: The system shall implement a 4th-order Runge-Kutta (RK4) explicit integration method as the default transient solver to integrate node temperatures over time step `dt`.
- **Verification Method**: Transient analytical comparison test case (`test_simulation.py::test_transient_cooling`).
- **Traceability Link**: `ThermalSystem::step_rk4` in `src/core/solver.cpp`.

### REQ-SYS-002: Stiff System Stability (Backward Euler Solver)
- **Description**: The system shall implement an implicit Backward Euler integration solver with Newton-Raphson iteration to solve stiff thermal loops (where time constant `tau = C / G` is smaller than the simulation step size `dt`).
- **Verification Method**: Stiff transient stability test case (`test_simulation.py::test_stiff_system_implicit`).
- **Traceability Link**: `ThermalSystem::step_backward_euler` in `src/core/solver.cpp`.

### REQ-SYS-003: Explicit Euler Solver
- **Description**: The system shall implement a basic first-order explicit Euler solver (`T(t + dt) = T(t) + dt * Q_net / C`) for fast, non-stiff thermal loop updates.
- **Verification Method**: Code review.
- **Traceability Link**: `ThermalSystem::step_explicit_euler` in `src/core/solver.cpp`.

### REQ-SYS-004: Steady-State Solver
- **Description**: The system shall provide a Newton-Raphson solver to compute steady-state thermal equilibrium (`Q_net = 0` for all non-fixed nodes) within a configurable convergence tolerance.
- **Verification Method**: Steady-state verification test case (`test_simulation.py::test_steady_state`).
- **Traceability Link**: `ThermalSystem::solve_steady_state` in `src/core/solver.cpp`.

---

## 2. Physics & Fluid/Solid Media Models (PHYS)

### REQ-PHYS-001: Conductive Heat Transfer
- **Description**: The system shall model conduction heat transfer between solid domains governed by Fourier's law: `Q = G * (T_A - T_B)`, where `G` is thermal conductance.
- **Verification Method**: Conduction balance test case (`test_simulation.py::test_steady_state`).
- **Traceability Link**: Conduction logic inside `ThermalSystem::compute_net_heat_rate` in `src/core/solver.cpp`.

### REQ-PHYS-002: Convective Heat Transfer
- **Description**: The system shall model convection heat transfer between a solid domain and a fluid domain governed by Newton's law of cooling: `Q = hA * (T_solid - T_fluid)`.
- **Verification Method**: Preset cooling loop tests.
- **Traceability Link**: Convection logic inside `ThermalSystem::compute_net_heat_rate` in `src/core/solver.cpp`.

### REQ-PHYS-003: Radiative Heat Transfer
- **Description**: The system shall model radiation heat transfer between domains governed by Stefan-Boltzmann: `Q = F_rad * (T_A^4 - T_B^4)`, where `F_rad` is the combined emissivity-area factor.
- **Verification Method**: Radiation test checks.
- **Traceability Link**: Radiation logic inside `ThermalSystem::compute_net_heat_rate` in `src/core/solver.cpp`.

### REQ-PHYS-004: Fluid Advection (Enthalpy Transport)
- **Description**: Coolant loops shall transport thermal energy via fluid advection using an upstream-biased scheme: `Q_net = m_dot * cp * (T_upstream - T_downstream)`.
- **Verification Method**: Fluid flow advection checks in loop tests.
- **Traceability Link**: Advection logic inside `ThermalSystem::compute_net_heat_rate` in `src/core/solver.cpp`.

### REQ-PHYS-005: Quadratic Fan / Pump Match
- **Description**: The flow rate inside advection links connected to fan/pump components shall be resolved matching a quadratic performance curve (`P_fan = P_max - B * v^2`) against system quadratic and linear resistance (`P_sys = K * v^2 + R * v`).
- **Verification Method**: Analytical solver validation.
- **Traceability Link**: Curve solution inside `ThermalSystem::GetLinkFlowRateAndDeriv` in `src/core/solver.cpp`.

### REQ-PHYS-006: Temperature-Dependent Fluid Properties
- **Description**: Fluid densities (`rho`) and specific heat capacities (`cp`) shall be evaluated as polynomial functions of the node's temperature. Supported built-in fluid models are Water, 50/50 Glycol, Engine Oil, Air, and Custom.
- **Verification Method**: Fluid property lookup test cases (`test_media_simulation.py`).
- **Traceability Link**: `GetDesktopNodeProperties` in `src/desktop/app_state.cpp` and `GetNodeCapacityAndDeriv` in `src/core/solver.cpp`.

### REQ-PHYS-007: Thermostat Flow-Splitting Valve
- **Description**: The bypass/main flow split ratio shall be calculated based on the thermostat temperature (opening begins at 82°C and reaches 100% full open to the radiator at 95°C).
- **Verification Method**: Coolant loop validation tests (`test_complete_loop.py`).
- **Traceability Link**: `UpdateThermostatBypassFlows` in `src/desktop/presets.cpp`.

---

## 3. Software Architecture & Operations (SW)

### REQ-SW-001: 5-Layer Software Boundaries
- **Description**: The codebase shall maintain strict layer separation (Physics Core ➔ Domain Model ➔ State ➔ Sync Bridge ➔ UI) and adhere to global variable prefixing conventions (`g_` prefix for all global state variables).
- **Verification Method**: Architecture audit.
- **Traceability Link**: Layer definitions in `docs/systems_architecture.md` and definitions in `src/desktop/app_state.cpp`.

### REQ-SW-002: Undo / Redo Command History Stack
- **Description**: The desktop application shall maintain an undo-redo stack of model state snapshots, limited to a maximum depth of 50 states to prevent memory exhaustion.
- **Verification Method**: Code review.
- **Traceability Link**: `PushUndoState`, `Undo`, and `Redo` in `src/desktop/app_state.cpp`.

### REQ-SW-003: Model Serialization
- **Description**: The model schematic, node parameters, connection links, and active sliders shall be serializable to/from standard JSON formatted files with the `.gtm` extension.
- **Verification Method**: Save/load validation.
- **Traceability Link**: `SaveModel` and `LoadModel` in `src/desktop/serialization.cpp`.
