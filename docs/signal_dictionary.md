# Signal Dictionary
## 1DSIM Central Variable Database (Data Dictionary)
*Owner: Rohan Kadam (Systems Architect) | Status: Released*

---

All global state variables (signals) in the desktop application are cataloged below. Before declaring or modifying a global variable in `src/desktop/app_state.cpp`, it must be documented in this registry.

---

## 1. Simulation Model Elements (Layer 3 State)

| Signal ID | Data Type | Units | Read Permission | Write Permission | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `g_nodes` | `std::vector<std::shared_ptr<DesktopNode>>` | N/A | All layers | `PlaceComponent`, `LoadPreset`, `LoadModel`, `Undo`/`Redo` | Array of placed thermal node components |
| `g_links` | `std::vector<std::shared_ptr<DesktopLink>>` | N/A | All layers | Canvas link, `LoadPreset`, `LoadModel`, `Undo`/`Redo` | Array of schematic connections (links) |
| `g_solver` | `ThermalSystem` | N/A | Sync Bridge | `SyncSystemWithSolver` | Underlying C++ numerical solver instance |
| `g_active_preset` | `std::string` | N/A | All layers | `LoadPreset` | Name ID of the currently loaded model template |

---

## 2. Selection & UI State (Layer 3 State)

| Signal ID | Data Type | Units | Read Permission | Write Permission | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `g_selected_node` | `std::shared_ptr<DesktopNode>` | N/A | Inspector, Canvas | Canvas selection click, Object Explorer, `Undo`/`Redo` | Currently highlighted/selected node |
| `g_selected_link` | `std::shared_ptr<DesktopLink>` | N/A | Inspector, Canvas | Canvas selection click, Object Explorer | Currently highlighted/selected link |
| `g_comp_mode` | `bool` | N/A | Canvas, Inspector | Object Explorer mode tab switch | `true` = Physical Component mode, `false` = Generic mode |

---

## 3. Solver Control Variables (Layer 3 State)

| Signal ID | Data Type | Units | Read Permission | Write Permission | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `g_is_running` | `bool` | N/A | WinMain loop | Run/Pause button, WinMain loop | `true` if active time-stepping is running |
| `g_sim_time` | `double` | `s` | UI toolbar, Plots | `StepSimulation`, `resetSimulation` | Simulation runtime clock |
| `g_time_step` | `double` | `s` | `StepSimulation` | Toolbar dt input | Integration time step (`dt`) |
| `g_solver_type` | `std::string` | N/A | `StepSimulation` | Toolbar solver combo | Solver algorithm choice ("rk4", "euler", "backward_euler") |
| `g_implicit_tolerance` | `float` | N/A | `StepSimulation` | Toolbar tolerance input | Newton tolerance for Backward Euler solver |
| `g_implicit_max_iter` | `int` | N/A | `StepSimulation` | Toolbar iter input | Max Newton iterations for Backward Euler solver |

---

## 4. History & Log State (Layer 3 State)

| Signal ID | Data Type | Units | Read Permission | Write Permission | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `g_time_history` | `std::vector<double>` | `s` | ImPlot | `UpdateHistory`, `ResetHistory` | Time coordinates for diagnostic plots |
| `g_temp_history` | `std::unordered_map<int, std::vector<double>>` | `°C` | ImPlot | `UpdateHistory`, `ResetHistory` | Node temperature histories keyed by ID |
| `g_plot_active_nodes` | `std::unordered_map<int, bool>` | N/A | Diagnostics UI | Plot checkboxes, Preset/Model loads | Visibility visibility switches for plot lines |
| `g_logs` | `std::vector<LogLine>` | N/A | Console UI | `Log` | Append-only database of system logs |

---

## 5. UI Sliders State (Layer 3 State)

| Signal ID | Data Type | Units | Read Permission | Write Permission | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `g_sliders` | `std::vector<SliderConfig>` | N/A | Inspector UI, Sliders | `LoadPreset`, `LoadModel`, Inspector panel | User-defined slider mappings to model variables |

---

## 6. Canvas & Tool State (Layer 3 State)

| Signal ID | Data Type | Units | Read Permission | Write Permission | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `g_canvas_scrolling` | `ImVec2` | `px` | Canvas render | Canvas mouse MMB drag | Center scrolling panning offset |
| `g_canvas_zoom` | `float` | N/A | Canvas render | Canvas mouse scroll wheel | Zoom coefficient |
| `g_grid_snap` | `bool` | N/A | Canvas, Placement | Toolbar snap toggle, Options menu | Grid snapping enablement flag |
| `g_current_tool` | `int` | N/A | Canvas render | `setTool`, Object Explorer | 0=Select, 1=Link, 2=Place component, 3=Connect ports |
| `isDragging` | `bool` | N/A | Canvas | Canvas mouse click | Dragging indicator state |
| `dragNode` | `std::shared_ptr<DesktopNode>` | N/A | Canvas | Canvas mouse click | Pointer to active dragged node |

---

## 7. Undo / Redo Stacks (Layer 3 State)

| Signal ID | Data Type | Units | Read Permission | Write Permission | Description |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `g_undo_stack` | `std::vector<ModelState>` | N/A | `Undo` | `PushUndoState`, `LoadModel` | Undo stack storing historical state snapshots |
| `g_redo_stack` | `std::vector<ModelState>` | N/A | `Redo` | `Undo`, `PushUndoState` (clears) | Redo stack |
