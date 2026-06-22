# Architecture

## Overview

1DSIM is split into four layers that communicate through well-defined interfaces:

```
┌──────────────────────────────────────────────────────────┐
│  Presentation Layer                                      │
│  ┌─────────────────────────┐  ┌────────────────────────┐ │
│  │  Web UI                 │  │  Desktop App            │ │
│  │  index.html / app.js    │  │  main.cpp (ImGui+DX11)  │ │
│  │  style.css / Chart.js   │  │  ImPlot charts          │ │
│  └──────────┬──────────────┘  └──────────┬─────────────┘ │
│             │ HTTP REST (JSON)            │ Direct call   │
└─────────────┼─────────────────────────────┼──────────────┘
              │                             │
┌─────────────▼─────────────────────────────▼──────────────┐
│  API / Wrapper Layer                                     │
│  ┌─────────────────────────┐  ┌────────────────────────┐ │
│  │  server.py              │  │  thermal_solver.py     │ │
│  │  HTTP REST endpoints    │  │  ctypes wrapper        │ │
│  │  /api/init, /step,      │  │  ThermalSystem class   │ │
│  │  /api/solve_steady      │  │                        │ │
│  └──────────┬──────────────┘  └──────────┬─────────────┘ │
└─────────────┼─────────────────────────────┼──────────────┘
              │ ctypes ABI                  │ ctypes ABI
┌─────────────▼─────────────────────────────▼──────────────┐
│  C++ Core (solver.dll)                                   │
│  ┌───────────────────────────────────────────────────┐   │
│  │  solver.h / solver.cpp / bindings.cpp             │   │
│  │  ThermalSystem, ThermalNode, ThermalLink          │   │
│  │  Physics: Conduction, Convection, Radiation,      │   │
│  │           Advection, Fan/Pump                     │   │
│  │  Fluids:  Water, Glycol, Oil, Air, Mixture, Custom│   │
│  │  Solvers: Euler, RK4, Backward Euler, Steady-State│   │
│  └───────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────┘
```

## Data Flow

### Transient Simulation (Web)

```
User clicks "Run"
  → app.js calls POST /api/init with nodes[] + links[] JSON
  → server.py deserialises, calls ThermalSystem.add_node() / add_link()
  → Per time step: POST /api/step with dt, solver type, current params
  → server.py calls step_rk4(dt) or step_explicit_euler(dt)
  → C++ solver integrates ODEs, returns new temperatures
  → app.js updates canvas colour map and Chart.js time plots
```

### Desktop App

```
User edits node/link in ImGui Attribute Sheet
  → main.cpp calls solver.update_node() / solver.update_link() directly
  → Per frame: solver.step_rk4(dt) called in render loop
  → Canvas redrawn with temperature gradients and flow chevrons
  → ImPlot panels update in real time
```

## Component Responsibilities

| Component | Responsibility |
|-----------|---------------|
| `solver.cpp` | All physics: ODE integration, fluid properties, fan matching |
| `bindings.cpp` | C-ABI DLL exports for ctypes |
| `thermal_solver.py` | Python-side ctypes wrapper, unit helpers |
| `server.py` | HTTP routing, JSON deserialisation, session state |
| `app.js` | Canvas rendering, simulation loop, UI event handling |
| `main.cpp` | ImGui layout, ImPlot rendering, file I/O, desktop state |
