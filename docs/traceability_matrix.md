# Codebase Traceability Matrix
## Mapping Requirements to Code Implementation and Test Verification
*Owner: Rohan Kadam (Systems Architect) | Status: Released*

---

This matrix links the requirements defined in [requirements.md](file:///d:/Work/Thermal%20Simulation/docs/requirements.md) to their C++ code implementations and Python validation tests.

| Requirement ID | Description | Subsystem | Code Implementation Reference | Python Verification Test | Verification Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`REQ-SYS-001`** | RK4 Integration Solver | Solver | [`solver.cpp:365`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L365) (`step_rk4`) | [`test_simulation.py::test_transient_cooling`](file:///d:/Work/Thermal%20Simulation/tests/test_simulation.py#L10) | **Verified** |
| **`REQ-SYS-002`** | Backward Euler Solver | Solver | [`solver.cpp:626`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L626) (`step_backward_euler`) | [`test_simulation.py::test_stiff_system_implicit`](file:///d:/Work/Thermal%20Simulation/tests/test_simulation.py#L74) | **Verified** |
| **`REQ-SYS-003`** | Explicit Euler Solver | Solver | [`solver.cpp:342`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L342) (`step_explicit_euler`) | Code Review / Standard | **Approved** |
| **`REQ-SYS-004`** | Steady-State Solver | Solver | [`solver.cpp:443`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L443) (`solve_steady_state`) | [`test_simulation.py::test_steady_state`](file:///d:/Work/Thermal%20Simulation/tests/test_simulation.py#L46) | **Verified** |
| **`REQ-PHYS-001`** | Conductive Heat Transfer | Equations | [`solver.cpp:278`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L278) (`compute_net_heat_rate`) | [`test_simulation.py::test_steady_state`](file:///d:/Work/Thermal%20Simulation/tests/test_simulation.py#L46) | **Verified** |
| **`REQ-PHYS-002`** | Convective Heat Transfer | Equations | [`solver.cpp:290`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L290) (`compute_net_heat_rate`) | [`test_complete_loop.py::test_complete_loop_preset`](file:///d:/Work/Thermal%20Simulation/tests/test_complete_loop.py#L10) | **Verified** |
| **`REQ-PHYS-003`** | Radiative Heat Transfer | Equations | [`solver.cpp:302`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L302) (`compute_net_heat_rate`) | [`test_simulation.py::test_temperature_dependence`](file:///d:/Work/Thermal%20Simulation/tests/test_simulation.py#L100) | **Verified** |
| **`REQ-PHYS-004`** | Fluid Advection Scheme | Equations | [`solver.cpp:314`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L314) (`compute_net_heat_rate`) | [`test_complete_loop.py::test_complete_loop_preset`](file:///d:/Work/Thermal%20Simulation/tests/test_complete_loop.py#L10) | **Verified** |
| **`REQ-PHYS-005`** | Quadratic Fan / Pump Match | Curves | [`solver.cpp:111`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L111) (`GetLinkFlowRateAndDeriv`) | [`test_complete_loop.py::test_complete_loop_preset`](file:///d:/Work/Thermal%20Simulation/tests/test_complete_loop.py#L10) | **Verified** |
| **`REQ-PHYS-006`** | Fluid Properties Polynomials | Fluids | [`app_state.cpp:32`](file:///d:/Work/Thermal%20Simulation/src/desktop/app_state.cpp#L32) (`GetDesktopNodeProperties`), [`solver.cpp:93`](file:///d:/Work/Thermal%20Simulation/src/core/solver.cpp#L93) (`GetNodeCapacityAndDeriv`) | [`test_media_simulation.py::test_water_properties`](file:///d:/Work/Thermal%20Simulation/tests/test_media_simulation.py#L10) | **Verified** |
| **`REQ-PHYS-007`** | Thermostat Flow Bypass Split | Valves | [`presets.cpp:6`](file:///d:/Work/Thermal%20Simulation/src/desktop/presets.cpp#L6) (`UpdateThermostatBypassFlows`) | [`test_complete_loop.py::test_complete_loop_preset`](file:///d:/Work/Thermal%20Simulation/tests/test_complete_loop.py#L10) | **Verified** |
| **`REQ-SW-001`** | 5-Layer Software Boundaries | Codebase | [`systems_architecture.md`](file:///d:/Work/Thermal%20Simulation/docs/systems_architecture.md), [`app_state.cpp:96`](file:///d:/Work/Thermal%20Simulation/src/desktop/app_state.cpp#L96) | Manual Audit | **Approved** |
| **`REQ-SW-002`** | Undo / Redo State Stack | State | [`app_state.cpp:275`](file:///d:/Work/Thermal%20Simulation/src/desktop/app_state.cpp#L275) (`PushUndoState`), `Undo`, `Redo` | Manual UI Test | **Verified** |
| **`REQ-SW-003`** | Model Serialization | Storage | [`serialization.cpp:9`](file:///d:/Work/Thermal%20Simulation/src/desktop/serialization.cpp#L9) (`SaveModel`), `LoadModel` | Manual UI Test | **Verified** |
