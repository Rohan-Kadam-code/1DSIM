# 1DSIM Codebase Architecture
## System Engineering Reference Document
*Owner: Rohan Kadam (Systems Architect) | Controls: Antigravity (Implementation)*
*Last updated: 2026-06-27 | Baseline: current build*

---

> **How to use this document**
> - Before adding any code: check what layer it belongs to, what signals it reads/writes
> - Before adding any global variable: add it here first — name, owner layer, who reads, who writes
> - Before any new function: define its interface here first (inputs → outputs)
> - This document IS the source of truth. The code is the implementation.

---

## Part 1 — Layer Architecture

The codebase is decomposed into **5 functional layers**. Each layer has strict rules about what it can see.

```
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 5 — UI / PRESENTATION                                    │
│  ui_panels.cpp, ui_canvas.cpp, main.cpp                        │
│  READS: State layer globals (display)                          │
│  WRITES: State layer globals (user input)                      │
│  CALLS: Sync layer functions only (never physics directly)     │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 4 — SYNC BRIDGE                                          │
│  app_state.cpp (orchestration fns), presets.cpp                │
│  READS: Domain Model + State globals                           │
│  WRITES: State globals + Physics Core (via g_solver)          │
│  PURPOSE: Translates UI intent into physics commands           │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 3 — STATE (Application Brain)                           │
│  app_state.h / app_state.cpp (global variables)               │
│  PURPOSE: Single source of truth for all runtime state        │
│  RULE: No physics logic. No rendering code.                   │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 2 — DOMAIN MODEL                                         │
│  sim_elements.h, component_library.h/.cpp                     │
│  PURPOSE: Desktop representation of simulation elements       │
│  RULE: No global state access. No solver calls.               │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 1 — PHYSICS CORE                                         │
│  src/core/solver.h/.cpp, src/core/bindings.cpp                │
│  PURPOSE: Pure math — ODEs, fluid properties, fan curves      │
│  RULE: No ImGui. No globals. No file I/O.                     │
└─────────────────────────────────────────────────────────────────┘

PERSISTENCE (cross-cutting):
  serialization.cpp — save/load LAYER 3 state to/from .gtm JSON
  presets.cpp       — initialize LAYER 3 state from named templates
```

---

## Part 2 — Signal / Variable Dictionary

Every global variable in the system, organized by functional group.
**Rule:** Before adding a new global, define it in this table first.

### 2.1 Model Data (Core Simulation Elements)

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_nodes` | `vector<shared_ptr<DesktopNode>>` | app_state.cpp:97 | 3 | PlaceComponent, LoadPreset, LoadModel, Undo/Redo | UI panels, Canvas, SyncSystemWithSolver | Primary model store |
| `g_links` | `vector<shared_ptr<DesktopLink>>` | app_state.cpp:98 | 3 | Modal confirm, LoadPreset, LoadModel, Undo/Redo | UI panels, Canvas, SyncSystemWithSolver | Primary link store |
| `g_solver` | `ThermalSystem` | app_state.cpp:99 | 3 | SyncSystemWithSolver | StepSimulation, SolveSteadyState, UpdateHistory | Physics engine instance |
| `g_active_preset` | `string` | app_state.cpp:102 | 3 | LoadPreset | Toolbar combo, UpdateThermostatBypassFlows | Current template key |

### 2.2 Selection State (UI Interaction)

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_selected_node` | `shared_ptr<DesktopNode>` | app_state.cpp:100 | 3 | Canvas click, Object Explorer, Undo/Redo | Inspector panel, Canvas (highlight), deleteSelected | nullptr = nothing selected |
| `g_selected_link` | `shared_ptr<DesktopLink>` | app_state.cpp:101 | 3 | Canvas click, Object Explorer | Inspector panel, Canvas (highlight), deleteSelected | nullptr = nothing selected |

### 2.3 Undo / Redo History

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_undo_stack` | `vector<ModelState>` | app_state.cpp:104 | 3 | PushUndoState | Undo, toolbar Undo btn enable | Max 50 entries |
| `g_redo_stack` | `vector<ModelState>` | app_state.cpp:105 | 3 | Undo | Redo, toolbar Redo btn enable | Cleared on new edit |
| `g_drag_backup` | `ModelState` | app_state.cpp:106 | 3 | Canvas drag start | Canvas drag cancel | Backup before drag |
| `g_drag_backup_valid` | `bool` | app_state.cpp:107 | 3 | Canvas drag start | Canvas drag cancel | Guard flag |

### 2.4 Simulation Control

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_is_running` | `bool` | app_state.cpp:109 | 3 | Toolbar Run/Pause, main.cpp loop | main.cpp loop, toolbar btn state | Master run flag |
| `g_sim_time` | `double` | app_state.cpp:110 | 3 | StepSimulation, resetSimulation | Toolbar clock display, UpdateHistory | Seconds |
| `g_time_step` | `double` | app_state.cpp:111 | 3 | Toolbar dt input | StepSimulation, SolveSteadyState | Default: 0.05s |
| `g_solver_type` | `string` | app_state.cpp:112 | 3 | Toolbar solver combo | StepSimulation | "rk4" / "euler" / "backward_euler" |
| `g_implicit_tolerance` | `float` | app_state.cpp:113 | 3 | Toolbar tolerance input | StepSimulation (backward Euler only) | Default: 1e-5 |
| `g_implicit_max_iter` | `int` | app_state.cpp:114 | 3 | Toolbar iter input | StepSimulation (backward Euler only) | Default: 50 |
| `g_sim_speed` | `int` | app_state.cpp:115 | 3 | *(currently unused in UI)* | main.cpp loop (steps per frame) | Default: 1 |

### 2.5 Time History (Plot Data)

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_time_history` | `vector<double>` | app_state.cpp:119 | 3 | UpdateHistory, ResetHistory | Diagnostics panel (ImPlot X-axis) | Appended each step |
| `g_temp_history` | `unordered_map<int,vector<double>>` | app_state.cpp:120 | 3 | UpdateHistory, ResetHistory | Diagnostics panel (ImPlot Y-axis) | nodeId → temp array |
| `g_plot_active_nodes` | `unordered_map<int,bool>` | app_state.cpp:121 | 3 | LoadPreset, LoadModel | Diagnostics panel checkboxes | nodeId → visible |

### 2.6 Log Console

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_logs` | `vector<LogLine>` | app_state.cpp:123 | 3 | Log() — called everywhere | Simulation Log Console panel | Append-only |

### 2.7 Sliders (Live Inputs)

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_sliders` | `vector<SliderConfig>` | app_state.cpp:124 | 3 | LoadPreset, LoadModel | Inspector panel, ApplySlidersToSystem, SyncSlidersFromSystem | User-facing parameter controls |

### 2.8 Canvas State

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_canvas_scrolling` | `ImVec2` | app_state.cpp:126 | 3 | Canvas MMB drag | DrawSchematicCanvas | Pan offset |
| `g_canvas_zoom` | `float` | app_state.cpp:127 | 3 | Canvas scroll wheel | DrawSchematicCanvas | Default: 1.0 |
| `g_grid_snap` | `bool` | app_state.cpp:116 | 3 | Toolbar toggle, Options menu | Canvas drag, PlaceComponent | Default: true |
| `GRID_SIZE` | `const int` | app_state.cpp:117 | 3 | — | Canvas drag snap | Fixed: 20px |

### 2.9 Active Tool State

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_current_tool` | `int` | app_state.cpp:128 | 3 | setTool(), Object Explorer tab click | DrawSchematicCanvas (controls mouse behavior) | 0:Select, 1:AddLink, 2:PlaceComp, 3:ConnectPorts |
| `g_linking_start_node` | `shared_ptr<DesktopNode>` | app_state.cpp:129 | 3 | Canvas (tool=1 first click) | Canvas (tool=1 second click → make link) | First node in link creation |
| `g_pending_link_type` | `int` | app_state.cpp:130 | 3 | Modal confirm, Inspector | Canvas link creation | 0:Cond,1:Conv,2:Rad,3:Flow,4:Fan |
| `g_temp_mouse_pos` | `ImVec2` | app_state.cpp:131 | 3 | Canvas mouse move | Canvas (ghost link preview) | Current mouse canvas pos |
| `isDragging` | `bool` | app_state.cpp:133 | 3 | Canvas LMB down/up | Canvas drag logic | ⚠ non-standard naming (no g_ prefix) |
| `dragNode` | `shared_ptr<DesktopNode>` | app_state.cpp:134 | 3 | Canvas LMB on node | Canvas drag update | ⚠ non-standard naming |
| `dragOffset` | `ImVec2` | app_state.cpp:135 | 3 | Canvas LMB down | Canvas drag update | ⚠ non-standard naming |

### 2.10 Link Creation Modal

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_show_link_modal` | `bool` | app_state.cpp:137 | 3 | Canvas (tool=1 second click) | RenderUI() (modal open gate) | Triggers link config popup |
| `g_modal_link_type` | `int` | app_state.cpp:138 | 3 | Modal UI | Modal confirm handler | Link type in modal |
| `g_modal_link_p1` | `double` | app_state.cpp:139 | 3 | Modal UI | Modal confirm handler | Default: 10.0 |
| `g_modal_link_p2` | `double` | app_state.cpp:140 | 3 | Modal UI | Modal confirm handler | Default: 1.0 |
| `g_modal_node_a_id` | `int` | app_state.cpp:141 | 3 | Canvas click sequence | Modal confirm handler | -1 = unset |
| `g_modal_node_b_id` | `int` | app_state.cpp:142 | 3 | Canvas click sequence | Modal confirm handler | -1 = unset |

### 2.11 Component Library Mode (Physical Library)

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_comp_mode` | `bool` | app_state.cpp:156 | 3 | Object Explorer tab switch | Canvas (selects draw/interact path), Inspector | false=Generic, true=Physical |
| `g_pending_comp_type` | `string` | app_state.cpp:144 | 3 | Object Explorer arm btn | Canvas (ghost preview, place on click) | defId of component being placed |
| `g_placing_component` | `bool` | app_state.cpp:145 | 3 | Object Explorer arm btn | Canvas (show ghost) | true = cursor dragging a component |
| `g_force_tab_generic` | `bool` | app_state.cpp:157 | 3 | LoadPreset (generic presets) | Object Explorer tab bar | Forces tab switch on next frame |
| `g_force_tab_component` | `bool` | app_state.cpp:158 | 3 | LoadPreset (component presets) | Object Explorer tab bar | Forces tab switch on next frame |
| `g_reset_dockspace` | `bool` | app_state.cpp:159 | 3 | View menu → Reset Layout | RenderUI dockspace builder | One-shot flag |
| `g_comp_instances` | `vector<CompInstance>` | app_state.cpp:161 | 3 | PlaceComponent | Canvas (comp mode draw), Object Explorer tree, Inspector | Placed physical components |
| `g_comp_connections` | `vector<CompConnection>` | app_state.cpp:162 | 3 | Canvas (tool=3 port connect) | Canvas (comp mode draw), Object Explorer tree, Inspector | Wired port connections |
| `g_sel_comp` | `CompInstance*` | app_state.cpp:163 | 3 | Object Explorer / canvas click | Inspector panel | Raw ptr — must be re-resolved after vector resize |
| `g_sel_conn` | `CompConnection*` | app_state.cpp:164 | 3 | Object Explorer click | Inspector panel | Raw ptr — same caveat |

### 2.12 Port Connection State (Tool=3)

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_conn_from_inst` | `int` | app_state.cpp:147 | 3 | Canvas (tool=3 first click) | Canvas (tool=3 second click → connect) | -1 = none |
| `g_conn_from_port` | `string` | app_state.cpp:148 | 3 | Canvas (tool=3 first click) | Canvas (tool=3 second click) | Port ID of source |
| `g_hovered_port_id` | `string` | app_state.cpp:149 | 3 | Canvas hover detection | Canvas (port highlight), Inspector tooltip | "" = none |
| `g_hovered_port_inst` | `int` | app_state.cpp:150 | 3 | Canvas hover detection | Canvas (port highlight) | -1 = none |

### 2.13 ID Counters

| Variable | Type | Defined In | Layer | Written By | Read By | Notes |
|----------|------|-----------|-------|------------|---------|-------|
| `g_next_inst_id` | `int` | app_state.cpp:153 | 3 | PlaceComponent (increments) | PlaceComponent (assigns to new instance) | Starts: 1000 |
| `g_next_conn_id` | `int` | app_state.cpp:154 | 3 | Canvas port connect (increments) | Canvas port connect | Starts: 2000 |
| `g_lib_tab` | `int` | app_state.cpp:152 | 3 | Object Explorer | Object Explorer | Currently unused (placeholder) |

---

## Part 3 — Function Catalogue

Every top-level function: what layer it belongs to, what it consumes and produces.

### 3.1 Physics Core — `ThermalSystem` (solver.cpp)

| Function | Inputs | Outputs | Notes |
|----------|--------|---------|-------|
| `add_node(id, name, T, C, Q, fixed, ...)` | node params | internal nodes[] update | Appends, never re-orders |
| `add_link(id, a, b, type, p1, p2, ...)` | link params | internal links[] update | — |
| `update_node(id, ...)` | node params | in-place update | Must exist or no-op |
| `update_link(id, ...)` | link params | in-place update | Must exist or no-op |
| `set_node_fluid_params(id, ...)` | fluid poly coefficients | in-place update | Fluid nodes only |
| `clear()` | — | empties nodes[], links[] | Called at start of every sync |
| `step_explicit_euler(dt)` | dt | nodes[].temperature updated | O(N), fast, conditionally stable |
| `step_rk4(dt)` | dt | nodes[].temperature updated | O(4N), default solver |
| `step_backward_euler(dt, tol, maxIter)` | dt, tolerance, iterations | nodes[].temperature updated | O(N²) per iteration, stiff-safe |
| `solve_steady_state(tol, maxIter)` | tolerance, iterations | nodes[].temperature updated | Returns iteration count |
| `get_node_temperature(id)` | node id | double (Kelvin) | — |
| `get_nodes()` | — | const vector<ThermalNode>& | Read-only snapshot |
| `compute_net_heat_rate(idx, temps)` | node index, temps vector | double Q_net [W] | Private, core physics |
| `GetNodeCapacityAndDeriv(node, T_k, C, dC)` | node, temp | C, dC/dT | Private, for Newton solvers |
| `GetLinkFlowRateAndDeriv(link, T_up_k, F, dF)` | link, upstream T | F, dF/dT | Private, fan/pump model |

### 3.2 Sync Bridge — `app_state.cpp` (orchestration)

**These are the most important functions — they connect UI intent to physics.**

| Function | Line | Reads | Writes | Calls | Purpose |
|----------|------|-------|--------|-------|---------|
| `SyncSystemWithSolver()` | 224 | g_nodes, g_links | g_solver | g_solver.clear(), add_node, add_link, GetInternalLinks, ResolveSolverNodeId | Rebuilds entire solver from desktop model. Called before every step. |
| `ApplySlidersToSystem()` | 378 | g_sliders | g_nodes[].temp/q_gen, g_links[].p1 | — | Pushes slider values into model state before sync |
| `SyncSlidersFromSystem()` | 330 | g_nodes, g_links | g_sliders[].value | — | Pulls current node/link values back into slider display |
| `UpdateThermostatBypassFlows()` | presets.cpp:6 | g_nodes (id=500,400), g_links | g_links[].p1 for loop links | — | Special thermostat logic — adjusts bypass flow split based on temp |
| `StepSimulation()` | 508 | g_solver_type, g_time_step | g_sim_time, g_nodes[].temp | ApplySlidersToSystem, UpdateThermostatBypassFlows, SyncSystemWithSolver, g_solver.step_xxx, UpdateHistory | **Main simulation tick** — full pipeline per frame |
| `SolveSteadyState()` | 537 | g_time_step | g_nodes[].temp | ApplySlidersToSystem, UpdateThermostatBypassFlows, SyncSystemWithSolver, g_solver.solve_steady_state | 10-iteration convergence loop for steady state |
| `SyncComponentsWithSolver()` | 218 | — | — | ResolvePointers, SyncSystemWithSolver, ResetHistory | Used when switching modes |
| `SyncSelection()` | 214 | — | — | ResolvePointers | Re-validates selection pointers |

### 3.3 State Management — `app_state.cpp`

| Function | Line | Reads | Writes | Purpose |
|----------|------|-------|--------|---------|
| `PushUndoState()` | 275 | g_nodes, g_links | g_undo_stack, g_redo_stack | Snapshots current model — call before any destructive edit |
| `Undo()` | 286 | g_undo_stack | g_nodes, g_links, g_redo_stack | Restores previous state + syncs solver |
| `Redo()` | 308 | g_redo_stack | g_nodes, g_links, g_undo_stack | Re-applies undone state |
| `ResolvePointers()` | 167 | g_nodes, g_links | g_selected_node, dragNode, g_linking_start_node, g_selected_link | Re-binds dangling raw pointers by ID after vector modification |
| `ResetHistory()` | 248 | g_nodes, g_sim_time | g_time_history, g_temp_history | Clears and seeds plot history with current state |
| `UpdateHistory()` | 492 | g_nodes, g_sim_time | g_time_history, g_temp_history | Appends one data point to plot history |
| `Log(message, type)` | 258 | — | g_logs | Appends timestamped log entry (types: info/success/warning/error) |
| `clearWorkspace()` | 606 | — | g_nodes, g_links, g_comp_instances, g_comp_connections, g_sliders, all selection | Full clear, no undo push (call PushUndoState first) |
| `resetSimulation()` | 621 | g_nodes[].temp_init | g_nodes[].temp, g_time_history, g_temp_history, g_sim_time | Restores temperatures to temp_init values |
| `deleteSelected()` | 635 | g_selected_node, g_selected_link | g_nodes, g_links (removal) | Removes selected element + all connected links |
| `setTool(tool)` | 666 | — | g_current_tool, g_linking_start_node, g_conn_from_inst | Sets active canvas tool, clears partial link state |
| `PlaceComponent(defId, cx, cy)` | 427 | defId string, cx/cy | g_nodes (push), g_next_inst_id | Creates correct DesktopNode subclass from string key |
| `ExportCSV()` | 556 | g_time_history, g_temp_history, g_nodes | filesystem | Saves history to CSV via Windows save dialog |
| `UpdateNodeCapacityFromMaterial(node)` | 20 | node.material, g_materials, node.mass | node.capacity, node.c_a1, node.c_a2 | Material property lookup |
| `GetDesktopNodeProperties(node, cap, ca1, ca2)` | 32 | node (all fluid/solid fields) | cap, ca1, ca2 (out params) | Computes thermal capacity at current temp |

### 3.4 Canvas / Interaction — `ui_canvas.cpp`

| Function | Line | Reads | Writes | Purpose |
|----------|------|-------|--------|---------|
| `DrawSchematicCanvas(dl, origin, size)` | 103 | ALL state (reads everything) | g_selected_node/link, isDragging, dragNode, g_canvas_scrolling/zoom, g_show_link_modal, g_modal_*, g_conn_from_inst/port, g_hovered_port_id/inst | Main canvas render + all mouse interaction |
| `getNodeAt(pos)` | 57 | g_nodes, g_canvas_scrolling, g_canvas_zoom | — | Hit-test canvas position → node |
| `getLinkAt(pos)` | 77 | g_links, g_nodes | — | Hit-test canvas position → link (by midpoint distance) |
| `GetPortPositions(node)` | 7 | node ports | — | Returns world positions of all ports |
| `ComputeOrthogonalRoute(portA, faceA, portB, faceB)` | 38 | port positions + faces | — | 2-segment L-shape routing between ports |
| `getDistanceToSegment(p, a, b)` | 69 | — | — | Pure math helper |

### 3.5 UI Panels — `ui_panels.cpp`

`RenderUI()` (L16) is the **only exported function**. Everything below is inline within it.

| Panel / Section | ImGui Window Name | Line | Reads | Writes | Notes |
|----------------|-------------------|------|-------|--------|-------|
| Main Window | `GT_MainWindow` | 50 | — | — | Host for menu + toolbar + dockspace |
| Menu Bar | *(inside GT_MainWindow)* | 54 | g_undo_stack, g_redo_stack, g_selected_* | — | File/Edit/Simulation/View/Options menus |
| Ribbon Toolbar | `RibbonToolbar` (child) | 121 | g_is_running, g_solver_type, g_sim_time, g_grid_snap | g_is_running, g_solver_type, g_time_step, g_implicit_* | 7 groups: File, Edit, Templates, Solver Control, Solver Options, Editor, Export |
| Dockspace | `MyDockSpace` | 338 | — | — | Programmatic layout on first run or reset |
| **Object Explorer** | `Object Explorer` | 383 | g_comp_mode, g_comp_instances, g_comp_connections, g_nodes, g_links | g_comp_mode, g_sel_comp, g_sel_conn, g_selected_node/link, g_pending_comp_type, g_placing_component, g_current_tool | Left panel. Two tabs: Physical Library / Generic Node-Link |
| **Schematic Canvas** | `Schematic Diagram Canvas` | 552 | *(delegates to DrawSchematicCanvas)* | *(same)* | Center panel — pure passthrough to ui_canvas.cpp |
| **Inspector Panel** | `Inspector Panel` | 563 | g_selected_node, g_selected_link, g_sel_comp, g_sel_conn, g_sliders | node/link property edits, g_sliders[].value | Right panel. Context-sensitive property sheet |
| **Diagnostics Summary** | `Diagnostics Summary` | 1351 | g_nodes, g_time_history, g_temp_history, g_plot_active_nodes | g_plot_active_nodes | Bottom-right. ImPlot temperature time charts |
| **Simulation Log Console** | `Simulation Log Console` | 1395 | g_logs | — | Bottom panel. Color-coded log output |

### 3.6 Persistence — `serialization.cpp` / `presets.cpp`

| Function | Reads | Writes | Serializes |
|----------|-------|--------|-----------|
| `SaveModel()` | g_nodes, g_links, g_sliders | filesystem (.gtm JSON) | nodes[], links[], sliders[] |
| `LoadModel()` | filesystem (.gtm JSON) | g_nodes, g_links, g_sliders, g_canvas_zoom/scrolling, g_plot_active_nodes | Reconstructs typed DesktopNode subclasses by name string |
| `LoadPreset(key)` | key string | g_nodes, g_links, g_sliders, g_active_preset, all canvas state | Hard-coded scenarios: "vehicle", "cpu", "battery", "window", "complete_loop" |
| `UpdateThermostatBypassFlows()` | g_nodes (id 500, 400), g_links | g_links[].p1 | Thermostat split logic — called every step when preset="complete_loop" |

---

## Part 4 — UI Window Inventory

Docked layout (default, saved in imgui.ini):

```
┌──────────────────────────────────────────────────────────────────┐
│  MENU BAR:  File | Edit | Simulation | View | Options            │
│  TOOLBAR:   [File] [Edit] [Templates] [Solver] [Options] [Clock] │
├──────────────┬───────────────────────────┬───────────────────────┤
│              │                           │  Inspector Panel      │
│   Object     │   Schematic Diagram       │  (right, 25% width)  │
│   Explorer   │   Canvas                  ├───────────────────────┤
│   (left 20%) │   (center)                │  Diagnostics Summary  │
│              │                           │  (right-bottom 35%)  │
├──────────────┴───────────────────────────┴───────────────────────┤
│  Simulation Log Console (bottom 25%)                             │
└──────────────────────────────────────────────────────────────────┘
```

**Object Explorer — Tab structure:**
- Tab 1: `Physical Library` → `g_comp_mode = true` → shows ICE component palette + placed components tree
- Tab 2: `Generic Node/Link` → `g_comp_mode = false` → shows generic node/link templates + model directory tree

**Inspector Panel — Context switching (what g_selected_* determines what you see):**
- `g_selected_node != null` → Node properties (name, temp, capacity, q_gen, material, domain, fluid params)
- `g_selected_link != null` → Link properties (type, p1, p2, fan params)
- `g_sel_comp != null` → CompInstance params (component-specific params map)
- `g_sel_conn != null` → CompConnection properties (flow_rate)
- nothing selected → Sliders panel (g_sliders list)

---

## Part 5 — Data Flow Map

### Per-Frame Simulation Tick (when `g_is_running = true`)

```
main.cpp::WinMain loop
  │
  ├─ for step in 0..g_sim_speed:
  │    StepSimulation()
  │      ├─ ApplySlidersToSystem()        sliders → g_nodes/g_links
  │      ├─ UpdateThermostatBypassFlows() g_nodes[500].temp → g_links[].p1
  │      ├─ SyncSystemWithSolver()        g_nodes/g_links → g_solver
  │      │    ├─ g_solver.clear()
  │      │    ├─ for each node: GetSolverNodes() → g_solver.add_node()
  │      │    │    [composite nodes expand here: EngineBlock→block+jacket]
  │      │    ├─ for each link: ResolveSolverNodeId() → g_solver.add_link()
  │      │    └─ for each node: GetInternalLinks() → g_solver.add_link()
  │      ├─ g_solver.step_rk4(g_time_step)  [or euler/backward_euler]
  │      ├─ g_sim_time += g_time_step
  │      ├─ for each node: n->temp = kToC(g_solver.get_node_temperature(n->id))
  │      └─ UpdateHistory()               g_nodes.temp → g_temp_history
  │
  └─ RenderUI()
       ├─ Toolbar: display g_sim_time, g_is_running state
       ├─ DrawSchematicCanvas():
       │    reads g_nodes[].temp → color gradient on nodes
       │    reads g_links[].p1 → flow chevron animation speed
       └─ Diagnostics: ImPlot from g_time_history, g_temp_history
```

### User Edit → Undo-safe Pattern

```
User action (place node, delete, edit param)
  │
  ├─ PushUndoState()         [always first]
  ├─ modify g_nodes / g_links
  ├─ SyncSystemWithSolver()  [keep solver consistent]
  └─ Log(...)                [always last]
```

---

## Part 6 — Known Issues / Technical Debt

> These are not bugs, they are architecture notes for future cleanup.

| Issue | Location | Impact | Fix in Phase |
|-------|----------|--------|-------------|
| `isDragging`, `dragNode`, `dragOffset` have no `g_` prefix | app_state.cpp:133-135 | Hard to identify as globals in a scan | Phase 2 rename |
| `g_sel_comp` and `g_sel_conn` are raw pointers into vectors | app_state.cpp:163-164 | Dangling pointer if vector resizes | Phase 2 — use IDs instead |
| `UpdateThermostatBypassFlows()` is hardcoded to node IDs 500, 400 | presets.cpp:6 | Breaks if any other preset uses those IDs | Phase 2 — generalize |
| `LoadModel()` identifies node types by name string | serialization.cpp:171 | Fragile: rename a node = wrong deserialization | Phase 2 — add `node_type` field to JSON |
| All panels are inlined in one 1508-line `RenderUI()` | ui_panels.cpp | Hard to find a panel's code | Phase 3 — split into panel functions |
| No node type field on `DesktopNode` (type identified by `dynamic_cast`) | sim_elements.h | Hard to serialize polymorphism | Phase 2 — add `virtual string GetTypeName()` |

---

## Part 7 — Design Rules (Contract)

**Before any code change, verify against these rules:**

1. **Layer boundaries**: UI code never calls `g_solver` directly. Only Sync Bridge functions do.
2. **Global creation**: Every new global variable must be added to the Signal Dictionary (Part 2) first.
3. **Function definition**: Every new function must define its interface in the Function Catalogue (Part 3) first.
4. **Undo safety**: Any function that modifies `g_nodes` or `g_links` must call `PushUndoState()` first.
5. **Solver consistency**: After any change to `g_nodes` or `g_links`, call `SyncSystemWithSolver()`.
6. **No magic IDs**: Never hardcode node/link IDs in new code without documenting them here.
7. **Naming convention**: All new globals must use `g_` prefix.
8. **Single responsibility**: Each function does one thing. If it does two, split it.

---

## Part 8 — Phase Roadmap (Architecture Evolution)

| Phase | Goal | Key Changes |
|-------|------|-------------|
| **Current (0)** | Baseline — document what exists | This document |
| **Phase 1** | Clean separation: split `RenderUI()` into per-panel functions | `DrawObjectExplorer()`, `DrawInspector()`, `DrawDiagnostics()`, `DrawLogConsole()` in separate files |
| **Phase 2** | Harden state: fix raw pointer issues, add node type field, generalize thermostat logic | `g_sel_comp` → `g_sel_comp_id`; `DesktopNode::GetTypeName()`; serialization type field |
| **Phase 3** | Sync scripts: separate sync layer into its own compilation unit | `src/desktop/sync.h/.cpp` owning all bridge functions |
| **Phase 4** | Interface contracts: formal layer headers with only allowed cross-layer calls documented | Per-layer include guards |

---

*This document must be updated before any code is added or changed.*
*If the code and this document disagree, the document wins.*
