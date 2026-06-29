#include "ui_panels.h"
#include "../../L3_state/app_state.h"
#include "../../L3_state/presets.h"
#include "../../L3_state/serialization.h"
#include "ui_canvas.h"
#include "../../L2_domain/component_library.h"
#include "../../../thirdparty/imgui/imgui.h"
#include "../../../thirdparty/imgui/imgui_internal.h"
#include "../../../thirdparty/implot/implot.h"
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <algorithm>

void RenderUI() {
    // Keyboard shortcuts
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            Undo();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
            Redo();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
            LoadModel();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
            SaveModel();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            deleteSelected();
        }
    }

    // Parent Main Window setup to host Menu, Toolbar, and Dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("GT_MainWindow", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    // 1. Menu Bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Model", "Ctrl+N")) {
                PushUndoState();
                g_nodes.clear();
                g_links.clear();
                g_selected_node = nullptr;
                g_selected_link = nullptr;
                g_sim_time = 0.0;
                SyncSystemWithSolver();
                ResetHistory();
                Log("New model schematic created.", "warning");
            }
            if (ImGui::MenuItem("Open Model...", "Ctrl+O")) {
                LoadModel();
            }
            if (ImGui::MenuItem("Save Model", "Ctrl+S")) {
                SaveModel();
            }
            if (ImGui::MenuItem("Clear Canvas")) {
                clearWorkspace();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", nullptr, !g_undo_stack.empty())) {
                Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", nullptr, !g_redo_stack.empty())) {
                Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected", "Delete", nullptr, (g_selected_node != nullptr || g_selected_link != nullptr))) {
                deleteSelected();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Simulation")) {
            if (ImGui::MenuItem("Run Simulation", "F5", &g_is_running)) {}
            if (ImGui::MenuItem("Solve Steady-State", "F6")) { SolveSteadyState(); }
            if (ImGui::MenuItem("Reset Solver")) { resetSimulation(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Layout")) {
                g_reset_dockspace = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options")) {
            ImGui::MenuItem("Grid Snapping", nullptr, &g_grid_snap);
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 250);
        ImGui::TextDisabled("GT-Thermal 1D - [Model: transient_cool.gtm]");
        ImGui::EndMenuBar();
    }
    
    // 2. Ribbon Toolbar
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.96f, 0.96f, 0.97f, 1.0f));
    ImGui::BeginChild("RibbonToolbar", ImVec2(0, 56.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // Toolbar Separator Helper Lambda
    auto DrawToolbarSeparator = []() {
        ImGui::SameLine();
        ImVec2 screen_pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(screen_pos.x, screen_pos.y),
            ImVec2(screen_pos.x, screen_pos.y + 40.0f),
            IM_COL32(200, 200, 205, 255),
            1.0f
        );
        ImGui::Dummy(ImVec2(8.0f, 0.0f));
        ImGui::SameLine();
    };

    // Group 1: MODEL FILE
    ImGui::BeginGroup();
    ImGui::TextDisabled("MODEL FILE");
    ImGui::Spacing();
    if (ImGui::Button("New")) {
        PushUndoState();
        g_nodes.clear();
        g_links.clear();
        g_selected_node = nullptr;
        g_selected_link = nullptr;
        g_sim_time = 0.0;
        SyncSystemWithSolver();
        ResetHistory();
        Log("New model schematic created.", "warning");
    }
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
        LoadModel();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        SaveModel();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        clearWorkspace();
    }
    ImGui::EndGroup();
    
    DrawToolbarSeparator();

    // Group 1.5: EDIT (Undo, Redo, Delete)
    ImGui::BeginGroup();
    ImGui::TextDisabled("EDIT");
    ImGui::Spacing();
    
    bool can_undo = !g_undo_stack.empty();
    if (!can_undo) ImGui::BeginDisabled();
    if (ImGui::Button("Undo")) { Undo(); }
    if (!can_undo) ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    bool can_redo = !g_redo_stack.empty();
    if (!can_redo) ImGui::BeginDisabled();
    if (ImGui::Button("Redo")) { Redo(); }
    if (!can_redo) ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    bool can_delete = (g_selected_node != nullptr || g_selected_link != nullptr);
    if (!can_delete) ImGui::BeginDisabled();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("Delete")) { deleteSelected(); }
    ImGui::PopStyleColor();
    if (!can_delete) ImGui::EndDisabled();
    
    ImGui::EndGroup();
    
    DrawToolbarSeparator();

    // Group 2: LIBRARY PRESETS
    ImGui::BeginGroup();
    ImGui::TextDisabled("LIBRARY TEMPLATES");
    ImGui::Spacing();
    ImGui::SetNextItemWidth(170.0f);
    if (ImGui::BeginCombo("##presetCombo", g_active_preset == "vehicle" ? "Vehicle Cooling Loop" : 
                        (g_active_preset == "cpu" ? "CPU Cooler Assembly" : 
                         (g_active_preset == "battery" ? "Li-Ion Battery Liquid" : "Double-Pane Window")))) {
        if (ImGui::Selectable("Vehicle Cooling Loop", g_active_preset == "vehicle")) LoadPreset("vehicle");
        if (ImGui::Selectable("CPU Cooler Assembly", g_active_preset == "cpu")) LoadPreset("cpu");
        if (ImGui::Selectable("Li-Ion Battery Liquid Cooling", g_active_preset == "battery")) LoadPreset("battery");
        if (ImGui::Selectable("Double-Pane Window Insulation", g_active_preset == "window")) LoadPreset("window");
        ImGui::EndCombo();
    }
    ImGui::EndGroup();

    DrawToolbarSeparator();

    // Group 3: SOLVER CONTROL
    ImGui::BeginGroup();
    ImGui::TextDisabled("SOLVER CONTROL");
    ImGui::Spacing();
    if (g_is_running) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.35f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.45f, 0.45f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button("Pause")) { g_is_running = false; Log("Simulation paused."); }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.95f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.9f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.4f, 0.1f, 1.0f));
        if (ImGui::Button("Run")) { g_is_running = true; Log("Simulation started."); }
        ImGui::PopStyleColor(3);
    }

    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        g_is_running = true;
        StepSimulation();
        g_is_running = false;
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.93f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.98f, 0.88f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.2f, 0.0f, 1.0f));
    if (ImGui::Button("Steady State")) { SolveSteadyState(); }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::Button("Reset")) { resetSimulation(); }
    ImGui::EndGroup();

    DrawToolbarSeparator();

    // Group 4: SOLVER OPTIONS
    ImGui::BeginGroup();
    ImGui::TextDisabled("SOLVER OPTIONS");
    ImGui::Spacing();
    ImGui::SetNextItemWidth(125.0f);
    std::string combo_label = "RK4";
    if (g_solver_type == "euler") combo_label = "Explicit Euler";
    else if (g_solver_type == "backward_euler") combo_label = "Implicit Euler";
    if (ImGui::BeginCombo("##solverCombo", combo_label.c_str())) {
        if (ImGui::Selectable("Runge-Kutta 4", g_solver_type == "rk4")) g_solver_type = "rk4";
        if (ImGui::Selectable("Explicit Euler", g_solver_type == "euler")) g_solver_type = "euler";
        if (ImGui::Selectable("Implicit Euler", g_solver_type == "backward_euler")) g_solver_type = "backward_euler";
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("dt (s):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    double dt_val = g_time_step;
    if (ImGui::InputDouble("##dtInput", &dt_val, 0.0f, 0.0f, "%.3f")) {
        if (dt_val > 0.0) g_time_step = dt_val;
    }
    
    if (g_solver_type == "backward_euler") {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Tol:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(65.0f);
        ImGui::InputFloat("##tolInput", &g_implicit_tolerance, 0.0f, 0.0f, "%.0e");
        
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Max Iter:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(45.0f);
        ImGui::InputInt("##maxIterInput", &g_implicit_max_iter, 0, 0);
    }
    ImGui::EndGroup();

    DrawToolbarSeparator();

    // Group 5: EDITOR
    ImGui::BeginGroup();
    ImGui::TextDisabled("EDITOR");
    ImGui::Spacing();
    if (g_grid_snap) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.90f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.85f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.02f, 0.36f, 0.63f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
        if (ImGui::Button("Grid Snap: ON")) { g_grid_snap = false; }
        ImGui::PopStyleColor(4);
    } else {
        if (ImGui::Button("Grid Snap: OFF")) { g_grid_snap = true; }
    }
    ImGui::EndGroup();

    DrawToolbarSeparator();

    // Group 6: EXPORT
    ImGui::BeginGroup();
    ImGui::TextDisabled("EXPORT");
    ImGui::Spacing();
    if (ImGui::Button("Export CSV")) { ExportCSV(); }
    ImGui::EndGroup();

    DrawToolbarSeparator();

    // Group 7: SIMULATION CLOCK
    ImGui::BeginGroup();
    ImGui::TextDisabled("SIMULATION CLOCK");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.02f, 0.36f, 0.63f, 1.0f));
    ImGui::Text("  %.2f s", g_sim_time);
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    
    // 3. DockSpace setup
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    
    // 4. Programmatic docking layout builder on first frame initialization or layout reset
    static bool dockspace_initialized = false;
    if (!dockspace_initialized || g_reset_dockspace) {
        bool reset_requested = g_reset_dockspace;
        dockspace_initialized = true;
        g_reset_dockspace = false;
        
        bool ini_exists = false;
        if (!reset_requested) {
            FILE* f = fopen("imgui.ini", "r");
            if (f) {
                ini_exists = true;
                fclose(f);
            }
        }
        
        if (reset_requested || !ini_exists) {
            // Clear any existing layout
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);
            
            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
            
            ImGuiID dock_id_right_bottom = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.35f, nullptr, &dock_id_right);
            
            ImGui::DockBuilderDockWindow("Object Explorer", dock_id_left);
            ImGui::DockBuilderDockWindow("Schematic Diagram Canvas", dock_main_id);
            ImGui::DockBuilderDockWindow("Simulation Log Console", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Inspector Panel", dock_id_right);
            ImGui::DockBuilderDockWindow("Diagnostics Summary", dock_id_right_bottom);
            
            ImGui::DockBuilderFinish(dockspace_id);
        }
    }
    
    ImGui::End(); // End Main Window

    // --- PANEL 2: COMPONENT LIBRARY & TREE EXPLORER (Left) ---
    ImGui::Begin("Object Explorer", nullptr);
    {
        // ΓöÇΓöÇ MODE SELECTOR (TABS) ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
        if (ImGui::BeginTabBar("ModeTabBar", ImGuiTabBarFlags_None)) {
            ImGuiTabItemFlags fComp = g_force_tab_component ? ImGuiTabItemFlags_SetSelected : 0;
            ImGuiTabItemFlags fGen  = g_force_tab_generic ? ImGuiTabItemFlags_SetSelected : 0;
            if (g_force_tab_component) g_force_tab_component = false;
            if (g_force_tab_generic)   g_force_tab_generic = false;

            if (ImGui::BeginTabItem("Physical Library", nullptr, fComp)) {
                if (!g_comp_mode) {
                    g_comp_mode = true;
                    SyncComponentsWithSolver();
                    SyncSelection();
                    Log("Switched to Physical Component Library mode.", "success");
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Generic Node/Link", nullptr, fGen)) {
                if (g_comp_mode) {
                    g_comp_mode = false;
                    SyncSelection();
                    Log("Switched to Generic Node/Link mode.", "info");
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::Spacing();
        ImGui::Separator();

        if (g_comp_mode) {
            // ΓöÇΓöÇ ICE COMPONENT LIBRARY ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
            ImGui::TextColored(ImVec4(0.9f,0.7f,0.2f,1.0f), "ICE Cooling Components");
            ImGui::TextDisabled("Click to arm, then click on canvas to place");
            ImGui::Spacing();

            const auto& lib = GetComponentLibrary();
            for (const auto& def : lib) {
                if (def.category != "ICE" && def.category != "Common") continue;
                bool armed = (g_pending_comp_type == def.id && g_placing_component);

                // Colour-code button by category
                ImVec4 btnCol  = armed ? ImVec4(0.2f,0.8f,0.4f,1.0f) : ImVec4(0.18f,0.22f,0.32f,1.0f);
                ImVec4 portCol;
                // Pick accent from border colour
                ImU32 bc = def.borderColor;
                portCol = ImVec4(((bc>>0)&0xFF)/255.0f,((bc>>8)&0xFF)/255.0f,((bc>>16)&0xFF)/255.0f,1.0f);

                ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f,0.35f,0.5f,1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, portCol);

                std::string btnLabel = def.name;
                if (ImGui::Button(btnLabel.c_str(), ImVec2(-1.0f, 32.0f))) {
                    g_pending_comp_type = def.id;
                    g_placing_component = true;
                    g_current_tool = 2;   // 2 = Place Component tool
                    Log("Armed: " + def.name + " ΓÇö click on canvas to place.", "info");
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", def.description.c_str());

                // Port legend inline
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80.0f);
                for (const auto& p : def.ports) {
                    ImVec4 pc;
                    ImU32 cc = PortTypeColor(p.type);
                    pc = ImVec4(((cc>>0)&0xFF)/255.0f,((cc>>8)&0xFF)/255.0f,((cc>>16)&0xFF)/255.0f,1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, pc);
                    ImGui::Text("o");
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                }
                ImGui::NewLine();

                ImGui::PopStyleColor(3);
                ImGui::Spacing();
            }

            ImGui::Separator();
            if (ImGui::Button("Connect Ports", ImVec2(-1,0))) {
                g_current_tool = 3;
                g_conn_from_inst = -1;
                g_conn_from_port = "";
                Log("Connect Ports tool active ΓÇö click a port, then click target port.", "info");
            }

            ImGui::Separator();
            // ΓöÇΓöÇ PLACED COMPONENTS TREE ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
            if (ImGui::CollapsingHeader("Placed Components", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto& inst : g_comp_instances) {
                    const ComponentDef* def2 = GetInstDef(inst);
                    std::string lbl = (def2 ? def2->name : inst.defId) + " [" + std::to_string(inst.instId) + "]";
                    bool sel = (g_sel_comp == &inst);
                    if (ImGui::Selectable(lbl.c_str(), sel)) {
                        g_sel_comp  = &inst;
                        g_sel_conn  = nullptr;
                        g_selected_node = nullptr;
                        g_selected_link = nullptr;
                        // mark selected
                        for (auto& i2 : g_comp_instances) i2.selected = false;
                        inst.selected = true;
                    }
                }
            }
            if (ImGui::CollapsingHeader("Connections")) {
                int ci = 0;
                for (auto& conn : g_comp_connections) {
                    std::string lbl = std::to_string(conn.fromInstId) + "." + conn.fromPortId
                                    + " -> " + std::to_string(conn.toInstId) + "." + conn.toPortId;
                    bool sel = (g_sel_conn == &conn);
                    if (ImGui::Selectable(lbl.c_str(), sel)) {
                        g_sel_conn  = &conn;
                        g_sel_comp  = nullptr;
                    }
                    ++ci;
                }
            }

        } else {
            // ΓöÇΓöÇ LEGACY GENERIC MODE ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
            if (ImGui::CollapsingHeader("Object Library Templates", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Columns(2, "lib_cols", false);
                ImGui::Button("[Node] Mass\n(Node)", ImVec2(90.0f, 40.0f));
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Double click on empty canvas to place a Thermal Mass node.");
                ImGui::NextColumn();
                ImGui::Button("[Fixed] Boundary\n(Fixed T)", ImVec2(90.0f, 40.0f));
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Double click to create, then toggle 'Fixed Temperature' in Attribute sheet.");
                ImGui::NextColumn();
                ImGui::Button("Conduction\n(Link)", ImVec2(90.0f, 30.0f));
                ImGui::NextColumn();
                ImGui::Button("Convection\n(Link)", ImVec2(90.0f, 30.0f));
                ImGui::NextColumn();
                ImGui::Button("Radiation\n(Link)", ImVec2(90.0f, 30.0f));
                ImGui::NextColumn();
                ImGui::Button("Flow Loop\n(Advection)", ImVec2(90.0f, 30.0f));
                ImGui::Columns(1);
            }
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Model Directory Tree", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::TreeNode("Active Nodes")) {
                    for (auto& n : g_nodes) {
                        bool selected = (g_selected_node == n);
                        if (ImGui::Selectable((std::string("[Node] ") + n->name).c_str(), selected)) {
                            g_selected_node = n;
                            g_selected_link = nullptr;
                        }
                    }
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("Active Connections")) {
                    for (auto& l : g_links) {
                        bool selected = (g_selected_link == l);
                        std::string typeStr = (l->type == 0 ? "Cond" : (l->type == 1 ? "Conv" : (l->type == 2 ? "Rad" : "Flow")));
                        std::string label = "[" + typeStr + "] (" + std::to_string(l->node_a) + " -> " + std::to_string(l->node_b) + ")";
                        if (ImGui::Selectable(label.c_str(), selected)) {
                            g_selected_link = l;
                            g_selected_node = nullptr;
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::End();

    // --- PANEL 3: SCHEMATIC CAD CANVAS (Center) ---
    ImGui::Begin("Schematic Diagram Canvas", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
        if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;
        DrawSchematicCanvas(ImGui::GetWindowDrawList(), canvas_pos, canvas_size);
    }
    ImGui::End();

    // --- PANEL 4: TABBED SIDEBAR (Right) ---
    ImGui::Begin("Inspector Panel", nullptr);
    {
        if (ImGui::BeginTabBar("InspectorTabBar")) {

            // Tab 1: Properties Spreadsheet (Attribute Sheet)
            if (ImGui::BeginTabItem("Attribute Sheet")) {
                ImGui::BeginChild("SS_ChildRegion");

                // ΓöÇΓöÇ COMPONENT LIBRARY ATTRIBUTE SHEET ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
                if (g_comp_mode && g_sel_comp != nullptr) {
                    const ComponentDef* def = GetInstDef(*g_sel_comp);
                    if (def) {
                        ImGui::TextColored(ImVec4(0.9f,0.7f,0.2f,1.0f), "%s  [ID: %d]",
                            def->name.c_str(), g_sel_comp->instId);
                        ImGui::TextDisabled("%s", def->description.c_str());
                        ImGui::Separator();

                        // Port legend
                        ImGui::TextDisabled("Ports:");
                        for (const auto& p : def->ports) {
                            ImU32 pc = PortTypeColor(p.type);
                            ImGui::TextColored(
                                ImVec4(((pc>>0)&0xFF)/255.f,((pc>>8)&0xFF)/255.f,((pc>>16)&0xFF)/255.f,1.f),
                                "  [%s] %s (%s)",
                                p.isOutput ? "OUT" : "IN", p.label.c_str(), PortTypeName(p.type));
                        }
                        ImGui::Separator();

                        // Parameter table
                        if (ImGui::BeginTable("comp_param_table", 4,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
                            ImGui::TableSetupColumn("Parameter",   ImGuiTableColumnFlags_WidthFixed, 120.f);
                            ImGui::TableSetupColumn("Value",       ImGuiTableColumnFlags_WidthFixed, 80.f);
                            ImGui::TableSetupColumn("Unit",        ImGuiTableColumnFlags_WidthFixed, 36.f);
                            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            for (const auto& pd : def->params) {
                                auto it = g_sel_comp->params.find(pd.key);
                                double val = (it != g_sel_comp->params.end()) ? it->second : pd.defaultValue;

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("%s", pd.label.c_str());
                                ImGui::TableNextColumn();
                                float fval = (float)val;
                                ImGui::SetNextItemWidth(-1);
                                std::string sliderLabel = "##comp_" + pd.key;
                                if (ImGui::SliderFloat(sliderLabel.c_str(), &fval,
                                    (float)pd.minVal, (float)pd.maxVal, "%.3g")) {
                                    g_sel_comp->params[pd.key] = fval;
                                    SyncComponentsWithSolver();
                                }
                                ImGui::TableNextColumn(); ImGui::TextDisabled("%s", pd.unit.c_str());
                                ImGui::TableNextColumn(); ImGui::TextDisabled("%s", pd.description.c_str());
                            }
                            ImGui::EndTable();
                        }

                        ImGui::Spacing();
                        // Delete component button
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f,0.1f,0.1f,1.0f));
                        if (ImGui::Button("Delete Component", ImVec2(-1,0))) {
                            PushUndoState();
                            int delId = g_sel_comp->instId;
                            g_comp_instances.erase(std::remove_if(g_comp_instances.begin(), g_comp_instances.end(),
                                [delId](const CompInstance& i){ return i.instId == delId; }), g_comp_instances.end());
                            g_comp_connections.erase(std::remove_if(g_comp_connections.begin(), g_comp_connections.end(),
                                [delId](const CompConnection& c){ return c.fromInstId==delId || c.toInstId==delId; }), g_comp_connections.end());
                            g_sel_comp = nullptr;
                            SyncComponentsWithSolver();
                            Log("Deleted component instance " + std::to_string(delId), "warning");
                        }
                        ImGui::PopStyleColor();
                    }
                }
                // ΓöÇΓöÇ LEGACY NODE/LINK ATTRIBUTE SHEET ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
                else if (!g_comp_mode || g_sel_comp == nullptr) {
                if (g_selected_node == nullptr && g_selected_link == nullptr) {
                    ImGui::TextDisabled("No component selected.");
                    ImGui::TextDisabled("Select an element on canvas to edit attributes.");
                } 
                else if (g_selected_node != nullptr) {
                    // Node Properties spreadsheet table
                    ImGui::TextColored(ImVec4(0.02f, 0.36f, 0.63f, 1.0f), "Node: [N:%d] %s", g_selected_node->id, g_selected_node->name.c_str());
                    ImGui::Separator();
                    
                    if (ImGui::BeginTable("node_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Unit", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();
                        
                        // ID
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Component ID");
                        ImGui::TableNextColumn(); ImGui::Text("%d", g_selected_node->id);
                        ImGui::TableNextColumn(); ImGui::Text("-");
                        ImGui::TableNextColumn(); ImGui::Text("Unique ID");
                        
                        // Name
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Label Name");
                        ImGui::TableNextColumn(); 
                        char nameBuf[64];
                        strcpy(nameBuf, g_selected_node->name.c_str());
                        ImGui::PushItemWidth(-FLT_MIN);
                        if (ImGui::InputText("##nameNodeInput", nameBuf, IM_ARRAYSIZE(nameBuf))) {
                            g_selected_node->name = nameBuf;
                            SyncSystemWithSolver();
                        }
                        if (ImGui::IsItemActivated()) { PushUndoState(); }
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn(); ImGui::Text("-");
                        ImGui::TableNextColumn(); ImGui::Text("Display tag");
                        
                        // Temp
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Temperature");
                        ImGui::TableNextColumn();
                        double temp_val = g_selected_node->temp;
                        ImGui::PushItemWidth(-FLT_MIN);
                        if (ImGui::InputDouble("##tempNodeInput", &temp_val, 0.1, 1.0, "%.2f")) {
                            g_selected_node->temp = temp_val;
                            g_selected_node->temp_init = temp_val;
                            SyncSlidersFromSystem();
                            SyncSystemWithSolver();
                            ResetHistory();
                        }
                        if (ImGui::IsItemActivated()) { PushUndoState(); }
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn(); ImGui::Text("C");
                        ImGui::TableNextColumn(); ImGui::Text("Celsius temp");
                        
                        // Domain Selection
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Domain");
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(-FLT_MIN);
                        const char* domains[] = { "Solid", "Fluid" };
                        int domain_idx = g_selected_node->domain;
                        if (ImGui::Combo("##domainCombo", &domain_idx, domains, 2)) {
                            PushUndoState();
                            g_selected_node->domain = domain_idx;
                            if (domain_idx == 0) {
                                UpdateNodeCapacityFromMaterial(g_selected_node);
                            }
                            SyncSystemWithSolver();
                        }
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn(); ImGui::Text("-");
                        ImGui::TableNextColumn(); ImGui::Text("Node physics domain");

                        if (g_selected_node->domain == 0) {
                            // Material
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn(); ImGui::Text("Material");
                            ImGui::TableNextColumn();
                            ImGui::PushItemWidth(-FLT_MIN);
                            if (ImGui::BeginCombo("##matNodeCombo", g_selected_node->material.c_str())) {
                                for (const auto& mat : g_materials) {
                                    bool is_selected = (g_selected_node->material == mat.name);
                                    if (ImGui::Selectable(mat.name.c_str(), is_selected)) {
                                        PushUndoState();
                                        g_selected_node->material = mat.name;
                                        UpdateNodeCapacityFromMaterial(g_selected_node);
                                        SyncSystemWithSolver();
                                    }
                                    if (is_selected) {
                                        ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndCombo();
                            }
                            ImGui::PopItemWidth();
                            ImGui::TableNextColumn(); ImGui::Text("-");
                            ImGui::TableNextColumn(); ImGui::Text("Material from database");
                            
                            // Mass (kg)
                            if (g_selected_node->material != "Custom") {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Mass");
                                ImGui::TableNextColumn();
                                double mass_val = g_selected_node->mass;
                                ImGui::PushItemWidth(-FLT_MIN);
                                if (ImGui::InputDouble("##massNodeInput", &mass_val, 0.1, 1.0, "%.2f")) {
                                    if (mass_val > 0.0) {
                                        g_selected_node->mass = mass_val;
                                        UpdateNodeCapacityFromMaterial(g_selected_node);
                                        SyncSystemWithSolver();
                                    }
                                }
                                if (ImGui::IsItemActivated()) { PushUndoState(); }
                                ImGui::PopItemWidth();
                                ImGui::TableNextColumn(); ImGui::Text("kg");
                                ImGui::TableNextColumn(); ImGui::Text("Mass of component");
                            }
                        } else {
                            // Fluid Medium
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn(); ImGui::Text("Fluid Medium");
                            ImGui::TableNextColumn();
                            ImGui::PushItemWidth(-FLT_MIN);
                            const char* mediums[] = { "Water", "Glycol", "Oil", "Air", "Mixture", "Custom" };
                            int med_idx = 0;
                            if (g_selected_node->fluid_medium == "Glycol") med_idx = 1;
                            else if (g_selected_node->fluid_medium == "Oil") med_idx = 2;
                            else if (g_selected_node->fluid_medium == "Air") med_idx = 3;
                            else if (g_selected_node->fluid_medium == "Mixture") med_idx = 4;
                            else if (g_selected_node->fluid_medium == "Custom") med_idx = 5;
                            if (ImGui::Combo("##fluidCombo", &med_idx, mediums, 6)) {
                                PushUndoState();
                                if (med_idx == 0) g_selected_node->fluid_medium = "Water";
                                else if (med_idx == 1) g_selected_node->fluid_medium = "Glycol";
                                else if (med_idx == 2) g_selected_node->fluid_medium = "Oil";
                                else if (med_idx == 3) g_selected_node->fluid_medium = "Air";
                                else if (med_idx == 4) g_selected_node->fluid_medium = "Mixture";
                                else if (med_idx == 5) g_selected_node->fluid_medium = "Custom";
                                SyncSystemWithSolver();
                            }
                            ImGui::PopItemWidth();
                            ImGui::TableNextColumn(); ImGui::Text("-");
                            ImGui::TableNextColumn(); ImGui::Text("Fluid type selection");

                            // Fluid Volume (Liters)
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn(); ImGui::Text("Fluid Volume");
                            ImGui::TableNextColumn();
                            double vol_val = g_selected_node->fluid_volume;
                            ImGui::PushItemWidth(-FLT_MIN);
                            if (ImGui::InputDouble("##volNodeInput", &vol_val, 0.1, 1.0, "%.2f")) {
                                if (vol_val > 0.0) {
                                    g_selected_node->fluid_volume = vol_val;
                                    SyncSystemWithSolver();
                                }
                            }
                            if (ImGui::IsItemActivated()) { PushUndoState(); }
                            ImGui::PopItemWidth();
                            ImGui::TableNextColumn(); ImGui::Text("L");
                            ImGui::TableNextColumn(); ImGui::Text("Fluid volume in Liters");

                            if (g_selected_node->fluid_medium == "Mixture") {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Glycol Conc.");
                                ImGui::TableNextColumn();
                                float mix_pct = (float)(g_selected_node->fluid_mix_ratio * 100.0);
                                ImGui::PushItemWidth(-FLT_MIN);
                                if (ImGui::SliderFloat("##mixRatioNodeInput", &mix_pct, 0.0f, 100.0f, "%.0f%%")) {
                                    g_selected_node->fluid_mix_ratio = (double)(mix_pct / 100.0f);
                                    SyncSystemWithSolver();
                                }
                                if (ImGui::IsItemActivated()) { PushUndoState(); }
                                ImGui::PopItemWidth();
                                ImGui::TableNextColumn(); ImGui::Text("%%");
                                ImGui::TableNextColumn(); ImGui::Text("Glycol concentration slider");
                            }
                            else if (g_selected_node->fluid_medium == "Custom") {
                                // rho_a0
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Density a0");
                                ImGui::TableNextColumn();
                                double val = g_selected_node->fluid_rho_a0;
                                ImGui::PushItemWidth(-FLT_MIN);
                                if (ImGui::InputDouble("##rhoA0Input", &val, 1.0, 10.0, "%.2f")) {
                                    g_selected_node->fluid_rho_a0 = val;
                                    SyncSystemWithSolver();
                                }
                                if (ImGui::IsItemActivated()) { PushUndoState(); }
                                ImGui::PopItemWidth();
                                ImGui::TableNextColumn(); ImGui::Text("kg/m3");
                                ImGui::TableNextColumn(); ImGui::Text("Constant density term");

                                // rho_a1
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Density a1");
                                ImGui::TableNextColumn();
                                val = g_selected_node->fluid_rho_a1;
                                ImGui::PushItemWidth(-FLT_MIN);
                                if (ImGui::InputDouble("##rhoA1Input", &val, 0.01, 0.1, "%.4f")) {
                                    g_selected_node->fluid_rho_a1 = val;
                                    SyncSystemWithSolver();
                                }
                                if (ImGui::IsItemActivated()) { PushUndoState(); }
                                ImGui::PopItemWidth();
                                ImGui::TableNextColumn(); ImGui::Text("kg/m3/K");
                                ImGui::TableNextColumn(); ImGui::Text("Linear density term");

                                // rho_a2
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Density a2");
                                ImGui::TableNextColumn();
                                val = g_selected_node->fluid_rho_a2;
                                ImGui::PushItemWidth(-FLT_MIN);
                                if (ImGui::InputDouble("##rhoA2Input", &val, 0.0001, 0.001, "%.6f")) {
                                    g_selected_node->fluid_rho_a2 = val;
                                    SyncSystemWithSolver();
                                }
                                if (ImGui::IsItemActivated()) { PushUndoState(); }
                                ImGui::PopItemWidth();
                                ImGui::TableNextColumn(); ImGui::Text("kg/m3/K2");
                                ImGui::TableNextColumn(); ImGui::Text("Quadratic density term");

                                // cp_a0
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Cp a0");
                                ImGui::TableNextColumn();
                                val = g_selected_node->fluid_cp_a0;
                                ImGui::PushItemWidth(-FLT_MIN);
                                if (ImGui::InputDouble("##cpA0Input", &val, 1.0, 10.0, "%.2f")) {
                                    g_selected_node->fluid_cp_a0 = val;
                                    SyncSystemWithSolver();
                                }
                                if (ImGui::IsItemActivated()) { PushUndoState(); }
                                ImGui::PopItemWidth();
                                ImGui::TableNextColumn(); ImGui::Text("J/kgK");
                                ImGui::TableNextColumn(); ImGui::Text("Constant specific heat term");

                                // cp_a1
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Cp a1");
                                ImGui::TableNextColumn();
                                val = g_selected_node->fluid_cp_a1;
                                ImGui::PushItemWidth(-FLT_MIN);
                                if (ImGui::InputDouble("##cpA1Input", &val, 0.01, 0.1, "%.4f")) {
                                    g_selected_node->fluid_cp_a1 = val;
                                    SyncSystemWithSolver();
                                }
                                if (ImGui::IsItemActivated()) { PushUndoState(); }
                                ImGui::PopItemWidth();
                                ImGui::TableNextColumn(); ImGui::Text("J/kgK2");
                                ImGui::TableNextColumn(); ImGui::Text("Linear specific heat term");

                                // cp_a2
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn(); ImGui::Text("Cp a2");
                                ImGui::TableNextColumn();
                                val = g_selected_node->fluid_cp_a2;
                                ImGui::PushItemWidth(-FLT_MIN);
                                if (ImGui::InputDouble("##cpA2Input", &val, 0.0001, 0.001, "%.6f")) {
                                    g_selected_node->fluid_cp_a2 = val;
                                    SyncSystemWithSolver();
                                }
                                if (ImGui::IsItemActivated()) { PushUndoState(); }
                                ImGui::PopItemWidth();
                                ImGui::TableNextColumn(); ImGui::Text("J/kgK3");
                                ImGui::TableNextColumn(); ImGui::Text("Quadratic specific heat term");
                            }
                        }
                        
                        // Capacity properties calculation
                        double cap_val = 0.0, c_a1_val = 0.0, c_a2_val = 0.0;
                        GetDesktopNodeProperties(g_selected_node, cap_val, c_a1_val, c_a2_val);
                        bool is_readonly = (g_selected_node->domain == 1) || (g_selected_node->material != "Custom");

                        // Capacity
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Heat Capacity");
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(-FLT_MIN);
                        if (is_readonly) {
                            ImGui::InputDouble("##capNodeInput", &cap_val, 0.0, 0.0, "%.1f", ImGuiInputTextFlags_ReadOnly);
                        } else {
                            if (ImGui::InputDouble("##capNodeInput", &cap_val, 10.0, 100.0, "%.0f")) {
                                if (cap_val > 0.0) g_selected_node->capacity = cap_val;
                                SyncSystemWithSolver();
                            }
                            if (ImGui::IsItemActivated()) { PushUndoState(); }
                        }
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn(); ImGui::Text("J/K");
                        ImGui::TableNextColumn(); ImGui::Text(g_selected_node->domain == 1 ? "Calculated capacity (rho*V*cp)" : (g_selected_node->material != "Custom" ? "Calculated capacity (m*cp0)" : "Constant capacity term a0"));
                        
                        // Capacity Linear term
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Cap Temp Coeff a1");
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(-FLT_MIN);
                        if (is_readonly) {
                            ImGui::InputDouble("##ca1NodeInput", &c_a1_val, 0.0, 0.0, "%.4f", ImGuiInputTextFlags_ReadOnly);
                        } else {
                            if (ImGui::InputDouble("##ca1NodeInput", &c_a1_val, 0.1, 1.0, "%.4f")) {
                                g_selected_node->c_a1 = c_a1_val;
                                SyncSystemWithSolver();
                            }
                            if (ImGui::IsItemActivated()) { PushUndoState(); }
                        }
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn(); ImGui::Text("J/K2");
                        ImGui::TableNextColumn(); ImGui::Text(g_selected_node->domain == 1 ? "Calculated coefficient (dC/dT)" : (g_selected_node->material != "Custom" ? "Calculated coefficient (m*cp1)" : "Linear temp coefficient a1"));
                        
                        // Capacity Quadratic term
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Cap Temp Coeff a2");
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(-FLT_MIN);
                        if (is_readonly) {
                            ImGui::InputDouble("##ca2NodeInput", &c_a2_val, 0.0, 0.0, "%.6f", ImGuiInputTextFlags_ReadOnly);
                        } else {
                            if (ImGui::InputDouble("##ca2NodeInput", &c_a2_val, 0.01, 0.1, "%.6f")) {
                                g_selected_node->c_a2 = c_a2_val;
                                SyncSystemWithSolver();
                            }
                            if (ImGui::IsItemActivated()) { PushUndoState(); }
                        }
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn(); ImGui::Text("J/K3");
                        ImGui::TableNextColumn(); ImGui::Text(g_selected_node->domain == 1 ? "Zero (Quadratic term)" : (g_selected_node->material != "Custom" ? "Calculated coefficient (m*cp2)" : "Quadratic temp coefficient a2"));
                        
                        // Heat Generation
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Heat Source");
                        ImGui::TableNextColumn();
                        double q_val = g_selected_node->q_gen;
                        ImGui::PushItemWidth(-FLT_MIN);
                        if (ImGui::InputDouble("##qNodeInput", &q_val, 5.0, 50.0, "%.0f")) {
                            g_selected_node->q_gen = q_val;
                            SyncSlidersFromSystem();
                            SyncSystemWithSolver();
                        }
                        if (ImGui::IsItemActivated()) { PushUndoState(); }
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn(); ImGui::Text("W");
                        ImGui::TableNextColumn(); ImGui::Text("Internal generation rate");
                        
                        // Fixed temp
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Fixed Bound");
                        ImGui::TableNextColumn();
                        bool is_fixed = g_selected_node->is_fixed;
                        if (ImGui::Checkbox("##fixedNodeChk", &is_fixed)) {
                            PushUndoState();
                            g_selected_node->is_fixed = is_fixed;
                            SyncSystemWithSolver();
                        }
                        ImGui::TableNextColumn(); ImGui::Text("bool");
                        ImGui::TableNextColumn(); ImGui::Text("Constant temperature constraint");
                        
                        ImGui::EndTable();
                    }
                    
                    if (ImGui::Button("Delete Component", ImVec2(-FLT_MIN, 25.0f))) {
                        deleteSelected();
                    }
                } 
                else if (g_selected_link != nullptr) {
                    ImGui::TextColored(ImVec4(0.02f, 0.36f, 0.63f, 1.0f), "Link ID: [L:%d] connecting N:%d -> N:%d", 
                                       g_selected_link->id, g_selected_link->node_a, g_selected_link->node_b);
                    ImGui::Separator();
                    
                    if (ImGui::BeginTable("link_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Unit", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();
                        
                        // ID
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Link ID");
                        ImGui::TableNextColumn(); ImGui::Text("%d", g_selected_link->id);
                        ImGui::TableNextColumn(); ImGui::Text("-");
                        ImGui::TableNextColumn(); ImGui::Text("Unique link ID");
                        
                        // Type
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("Physics Type");
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(-FLT_MIN);
                        int current_type = g_selected_link->type;
                        if (ImGui::BeginCombo("##linkTypeCombo", current_type == 0 ? "Conduction" : 
                                              (current_type == 1 ? "Convection" : 
                                               (current_type == 2 ? "Radiation" : 
                                                (current_type == 3 ? "Coolant Flow" : "Fan / Pump Link"))))) {
                            if (ImGui::Selectable("Conduction", current_type == 0)) { PushUndoState(); g_selected_link->type = 0; SyncSlidersFromSystem(); SyncSystemWithSolver(); }
                            if (ImGui::Selectable("Convection", current_type == 1)) { PushUndoState(); g_selected_link->type = 1; SyncSlidersFromSystem(); SyncSystemWithSolver(); }
                            if (ImGui::Selectable("Radiation", current_type == 2))  { PushUndoState(); g_selected_link->type = 2; SyncSlidersFromSystem(); SyncSystemWithSolver(); }
                            if (ImGui::Selectable("Coolant Flow", current_type == 3)) { PushUndoState(); g_selected_link->type = 3; SyncSlidersFromSystem(); SyncSystemWithSolver(); }
                            if (ImGui::Selectable("Fan / Pump Link", current_type == 4)) {
                                PushUndoState();
                                g_selected_link->type = 4;
                                if (g_selected_link->p1 == 0.0 || g_selected_link->p1 == 10.0 || g_selected_link->p1 == 400.0) g_selected_link->p1 = 1000.0;
                                if (g_selected_link->p2 == 0.0 || g_selected_link->p2 == 1.0) g_selected_link->p2 = 10.0;
                                SyncSlidersFromSystem();
                                SyncSystemWithSolver();
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn(); ImGui::Text("-");
                        ImGui::TableNextColumn(); ImGui::Text("0:Cond, 1:Conv, 2:Rad, 3:Flow, 4:Fan");
                        
                        // Parameter 1
                        std::string p1Label = "Conductance";
                        std::string p1Unit = "W/K";
                        std::string p1Desc = "Conduction factor G";
                        if (g_selected_link->type == 1) { p1Label = "Convection hA"; p1Desc = "Convection factor h*A"; }
                        else if (g_selected_link->type == 2) { p1Label = "Radiation G_rad"; p1Unit = "W/K4"; p1Desc = "Stefan-Boltzmann exchange (s*e*A)"; }
                        else if (g_selected_link->type == 3) {
                            p1Label = "Flow Rate";
                            int up_id = (g_selected_link->p2 >= 0.0) ? g_selected_link->node_a : g_selected_link->node_b;
                            auto it = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == up_id; });
                            if (it != g_nodes.end() && (*it)->domain == 1) {
                                p1Unit = "L/min";
                                p1Desc = "Volumetric coolant flow rate";
                            } else {
                                p1Unit = "W/K";
                                p1Desc = "Flow thermal capacity rate (m_dot*cp)";
                            }
                        } else if (g_selected_link->type == 4) {
                            p1Label = "Shut-off Pressure";
                            p1Unit = "Pa";
                            p1Desc = "Maximum static pressure (P_max)";
                        }
                        
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(p1Label.c_str());
                        ImGui::TableNextColumn();
                        double p1_val = g_selected_link->p1;
                        ImGui::PushItemWidth(-FLT_MIN);
                        if (ImGui::InputDouble("##p1LinkInput", &p1_val, 1.0, 10.0, "%.2f")) {
                            g_selected_link->p1 = p1_val;
                            SyncSlidersFromSystem();
                            SyncSystemWithSolver();
                        }
                        if (ImGui::IsItemActivated()) { PushUndoState(); }
                        ImGui::PopItemWidth();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(p1Unit.c_str());
                        ImGui::TableNextColumn(); ImGui::TextUnformatted((p1Desc + " (a0 term)").c_str());
                        
                        // Parameter 1 Linear term
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(g_selected_link->type == 4 ? "Resist Coeff K" : (p1Label + " Coeff a1").c_str());
                        ImGui::TableNextColumn();
                        double g_a1_val = g_selected_link->g_a1;
                        ImGui::PushItemWidth(-FLT_MIN);
                        if (ImGui::InputDouble("##ga1LinkInput", &g_a1_val, 0.1, 1.0, "%.4f")) {
                            g_selected_link->g_a1 = g_a1_val;
                            SyncSystemWithSolver();
                        }
                        if (ImGui::IsItemActivated()) { PushUndoState(); }
                        ImGui::PopItemWidth();
                        std::string g_a1Unit = g_selected_link->type == 4 ? "Pa/(m/s)2" : p1Unit + "/K";
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(g_a1Unit.c_str());
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(g_selected_link->type == 4 ? "System quadratic flow resistance" : "Linear temp coefficient a1");
                        
                        // Parameter 1 Quadratic term
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(g_selected_link->type == 4 ? "Resist Coeff R" : (p1Label + " Coeff a2").c_str());
                        ImGui::TableNextColumn();
                        double g_a2_val = g_selected_link->g_a2;
                        ImGui::PushItemWidth(-FLT_MIN);
                        if (ImGui::InputDouble("##ga2LinkInput", &g_a2_val, g_selected_link->type == 4 ? 0.1 : 0.01, g_selected_link->type == 4 ? 1.0 : 0.1, g_selected_link->type == 4 ? "%.4f" : "%.6f")) {
                            g_selected_link->g_a2 = g_a2_val;
                            SyncSystemWithSolver();
                        }
                        if (ImGui::IsItemActivated()) { PushUndoState(); }
                        ImGui::PopItemWidth();
                        std::string g_a2Unit = g_selected_link->type == 4 ? "Pa/(m/s)" : p1Unit + "/K2";
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(g_a2Unit.c_str());
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(g_selected_link->type == 4 ? "System linear flow resistance" : "Quadratic temp coefficient a2");
                        
                        // Parameter 2 (Flow direction or v_max)
                        if (g_selected_link->type == 3) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn(); ImGui::Text("Flow Dir");
                            ImGui::TableNextColumn();
                            ImGui::PushItemWidth(-FLT_MIN);
                            int dir_idx = g_selected_link->p2 >= 0.0 ? 0 : 1;
                            if (ImGui::BeginCombo("##dirCombo", dir_idx == 0 ? "Node A -> Node B" : "Node B -> Node A")) {
                                if (ImGui::Selectable("Node A -> Node B", dir_idx == 0)) { PushUndoState(); g_selected_link->p2 = 1.0; SyncSystemWithSolver(); }
                                if (ImGui::Selectable("Node B -> Node A", dir_idx == 1)) { PushUndoState(); g_selected_link->p2 = -1.0; SyncSystemWithSolver(); }
                                ImGui::EndCombo();
                            }
                            ImGui::PopItemWidth();
                            ImGui::TableNextColumn(); ImGui::Text("dir");
                            ImGui::TableNextColumn(); ImGui::Text("Coolant transport direction");
                        } else if (g_selected_link->type == 4) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn(); ImGui::Text("Free Velocity");
                            ImGui::TableNextColumn();
                            double p2_val = g_selected_link->p2;
                            ImGui::PushItemWidth(-FLT_MIN);
                            if (ImGui::InputDouble("##p2LinkInput", &p2_val, 0.1, 1.0, "%.2f")) {
                                g_selected_link->p2 = p2_val;
                                SyncSystemWithSolver();
                            }
                            if (ImGui::IsItemActivated()) { PushUndoState(); }
                            ImGui::PopItemWidth();
                            ImGui::TableNextColumn(); ImGui::Text("m/s");
                            ImGui::TableNextColumn(); ImGui::Text("Maximum fluid velocity (v_max)");
                            
                            // fan_area
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn(); ImGui::Text("Flow Area");
                            ImGui::TableNextColumn();
                            double area_val = g_selected_link->fan_area;
                            ImGui::PushItemWidth(-FLT_MIN);
                            if (ImGui::InputDouble("##areaLinkInput", &area_val, 0.0001, 0.001, "%.4f")) {
                                if (area_val > 0.0) g_selected_link->fan_area = area_val;
                                SyncSystemWithSolver();
                            }
                            if (ImGui::IsItemActivated()) { PushUndoState(); }
                            ImGui::PopItemWidth();
                            ImGui::TableNextColumn(); ImGui::Text("m2");
                            ImGui::TableNextColumn(); ImGui::Text("Cross-sectional area A");
                        }
                        
                        ImGui::EndTable();
                    }
                    
                    if (ImGui::Button("Delete Connection", ImVec2(-FLT_MIN, 25.0f))) {
                        deleteSelected();
                    }
                }
                } // end else if legacy mode

                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            
            // Tab 2: Time Plots (ImPlot)
            if (ImGui::BeginTabItem("Time Plots")) {
                if (ImGui::Button("Clear Time History")) {
                    g_time_history.clear();
                    g_temp_history.clear();
                    Log("Simulation plots cleared.");
                }
                ImGui::SameLine();
                if (ImGui::Button("Print History to Log")) {
                    for (const auto& n : g_nodes) {
                        auto& temps = g_temp_history[n->id];
                        std::string msg = n->name + " history (size=" + std::to_string(temps.size()) + "): ";
                        for (size_t i = 0; i < std::min(temps.size(), (size_t)10); ++i) {
                            msg += std::to_string(temps[i]) + ", ";
                        }
                        Log(msg);
                    }
                }
                
                ImGui::Text("Toggled Series:");
                ImGui::BeginChild("PlotSelectorsList", ImVec2(0, 70), true);
                for (auto& n : g_nodes) {
                    bool visible = g_plot_active_nodes[n->id];
                    if (ImGui::Checkbox(n->name.c_str(), &visible)) {
                        g_plot_active_nodes[n->id] = visible;
                    }
                    ImGui::SameLine(180.0f);
                    ImGui::TextDisabled("N:%d", n->id);
                }
                ImGui::EndChild();
                
                // Draw chart
                if (ImPlot::BeginPlot("Temperature History", ImVec2(-FLT_MIN, -FLT_MIN))) {
                    ImPlot::SetupAxes("Time (s)", "Temperature (C)", ImPlotAxisFlags_None, ImPlotAxisFlags_None);
                    
                    double x_min = g_time_history.empty() ? 0.0 : g_time_history.front();
                    double x_max = g_time_history.empty() ? 10.0 : g_time_history.back();
                    if (x_max - x_min < 5.0) x_max = x_min + 5.0;
                    
                    // Dynamic auto-fitting for Y-axis
                    double y_min = 10.0;
                    double y_max = 95.0;
                    bool has_data = false;
                    for (const auto& n : g_nodes) {
                        if (!g_plot_active_nodes[n->id]) continue;
                        auto& temps = g_temp_history[n->id];
                        for (double t : temps) {
                            if (!has_data) {
                                y_min = t;
                                y_max = t;
                                has_data = true;
                            } else {
                                if (t < y_min) y_min = t;
                                if (t > y_max) y_max = t;
                            }
                        }
                    }
                    if (has_data) {
                        if (y_max - y_min < 5.0) {
                            y_min -= 5.0;
                            y_max += 5.0;
                        } else {
                            double padding = 0.1 * (y_max - y_min);
                            y_min -= padding;
                            y_max += padding;
                        }
                    }
                    
                    ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max, g_is_running ? ImGuiCond_Always : ImGuiCond_Once);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, y_min, y_max, g_is_running ? ImGuiCond_Always : ImGuiCond_Once);
                    
                    for (const auto& n : g_nodes) {
                        if (!g_plot_active_nodes[n->id]) continue;
                        auto& temps = g_temp_history[n->id];
                        if (!temps.empty() && !g_time_history.empty()) {
                            ImPlot::PlotLine(n->name.c_str(), g_time_history.data(), temps.data(), (int)g_time_history.size());
                        }
                    }
                    ImPlot::EndPlot();
                }
                ImGui::EndTabItem();
            }
            
            // Tab: Fan Matcher (Dear ImGui & ImPlot)
            if (g_selected_link != nullptr && g_selected_link->type == 4) {
                if (ImGui::BeginTabItem("Fan Matcher")) {
                    double Pmax = std::abs(g_selected_link->p1);
                    double Vmax = std::abs(g_selected_link->p2);
                    double K = std::abs(g_selected_link->g_a1);
                    double R = std::abs(g_selected_link->g_a2);
                    double A = g_selected_link->fan_area;
                    
                    double v_oper = 0.0;
                    double Q_lmin = 0.0;
                    double P_oper = 0.0;
                    
                    if (Vmax > 0.0) {
                        double B = Pmax / (Vmax * Vmax);
                        v_oper = (-R + std::sqrt(R * R + 4.0 * (K + B) * Pmax)) / (2.0 * (K + B));
                        Q_lmin = v_oper * A * 60000.0;
                        P_oper = Pmax - B * v_oper * v_oper;
                    }
                    
                    ImGui::Text("Operating Point Calculations:");
                    ImGui::Text("Operating Velocity: %.2f m/s", (g_selected_link->p2 >= 0.0 ? 1.0 : -1.0) * v_oper);
                    ImGui::Text("Volumetric Flow Rate: %.2f L/min", (g_selected_link->p2 >= 0.0 ? 1.0 : -1.0) * Q_lmin);
                    ImGui::Text("Operating Pressure: %.1f Pa", P_oper);
                    ImGui::Separator();
                    
                    if (ImPlot::BeginPlot("Fan Performance Curve Matching", ImVec2(-FLT_MIN, -FLT_MIN))) {
                        ImPlot::SetupAxes("Velocity (m/s)", "Pressure (Pa)", ImPlotAxisFlags_None, ImPlotAxisFlags_None);
                        
                        double limitV = Vmax > 0.0 ? Vmax * 1.25 : 10.0;
                        double B_val = Vmax > 0.0 ? Pmax / (Vmax * Vmax) : 0.0;
                        
                        std::vector<double> v_plot(51);
                        std::vector<double> p_fan_plot(51);
                        std::vector<double> p_sys_plot(51);
                        for (int i = 0; i <= 50; ++i) {
                            double v = (limitV * i) / 50.0;
                            v_plot[i] = v;
                            p_fan_plot[i] = std::max(0.0, Pmax - B_val * v * v);
                            p_sys_plot[i] = K * v * v + R * v;
                        }
                        
                        ImPlot::PlotLine("Fan Pressure Rise (P_fan)", v_plot.data(), p_fan_plot.data(), 51);
                        ImPlot::PlotLine("System Resistance (P_sys)", v_plot.data(), p_sys_plot.data(), 51);
                        
                        double x_point[1] = { v_oper };
                        double y_point[1] = { P_oper };
                        ImPlot::PlotScatter("Operating Point", x_point, y_point, 1);
                        
                        ImPlot::EndPlot();
                    }
                    ImGui::EndTabItem();
                }
            }
            
            // Tab 3: Dynamic Preset Variables
            if (ImGui::BeginTabItem("Dynamic Variables")) {
                ImGui::BeginChild("Vars_ChildRegion");
                
                if (g_sliders.empty()) {
                    ImGui::TextDisabled("No scenario variables configured for this model.");
                } else {
                    for (auto& slide : g_sliders) {
                        std::string label = slide.label;
                        float val = slide.value;
                        if (ImGui::SliderFloat(label.c_str(), &val, slide.min_val, slide.max_val, "%.1f")) {
                            slide.value = val;
                            ApplySlidersToSystem();
                            SyncSystemWithSolver();
                        }
                        if (ImGui::IsItemActivated()) { PushUndoState(); }
                        ImGui::Spacing();
                    }
                }
                
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    // --- PANEL 5: DIAGNOSTICS STATS (Right Bottom) ---
    ImGui::Begin("Diagnostics Summary", nullptr);
    {
        if (ImGui::BeginTable("diagnostics_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableHeadersRow();
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Total Components");
            ImGui::TableNextColumn(); ImGui::Text("%zu", g_nodes.size());
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Active Connections");
            ImGui::TableNextColumn(); ImGui::Text("%zu", g_links.size());
            
            if (!g_nodes.empty()) {
                double maxT = -1e9, minT = 1e9;
                for (const auto& n : g_nodes) {
                    if (n->temp > maxT) maxT = n->temp;
                    if (n->temp < minT) minT = n->temp;
                }
                
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Max System Temp");
                ImGui::TableNextColumn(); ImGui::Text("%.2f C", maxT);
                
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Min System Temp");
                ImGui::TableNextColumn(); ImGui::Text("%.2f C", minT);
                
                double totalQ = 0.0;
                for (const auto& n : g_nodes) {
                    if (!n->is_fixed) totalQ += n->q_gen;
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("Net Heater Output");
                ImGui::TableNextColumn(); ImGui::Text("%.2f kW", totalQ / 1000.0);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();

    // --- PANEL 6: RUNTIME LOG OUTPUT CONSOLE (Bottom) ---
    ImGui::Begin("Simulation Log Console", nullptr);
    {
        if (ImGui::Button("Clear Log")) {
            g_logs.clear();
        }
        ImGui::Separator();
        
        ImGui::BeginChild("LogLinesRegion");
        for (const auto& line : g_logs) {
            ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // Default grey info
            if (line.type == "success")     color = ImVec4(0.1f, 0.6f, 0.1f, 1.0f);  // green
            else if (line.type == "warning") color = ImVec4(0.8f, 0.5f, 0.0f, 1.0f);  // orange
            else if (line.type == "error")   color = ImVec4(0.8f, 0.1f, 0.1f, 1.0f);  // red
            
            ImGui::TextDisabled("[%s]", line.time.c_str());
            ImGui::SameLine();
            ImGui::TextColored(color, "%s", line.message.c_str());
        }
        // Auto scroll to bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
            
        ImGui::EndChild();
    }
    ImGui::End();

    // --- POPUP MODAL: CREATE CONNECTION LINK ---
    if (g_show_link_modal) {
        ImGui::OpenPopup("Configure Link Connection");
    }
    
    if (ImGui::BeginPopupModal("Configure Link Connection", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Connecting Node %d to Node %d", g_modal_node_a_id, g_modal_node_b_id);
        ImGui::Separator();
        
        if (ImGui::BeginCombo("Heat Transfer Type", g_modal_link_type == 0 ? "Conduction" : 
                            (g_modal_link_type == 1 ? "Convection" : 
                             (g_modal_link_type == 2 ? "Radiation" : 
                              (g_modal_link_type == 3 ? "Coolant Flow" : "Fan / Pump Link"))))) {
            if (ImGui::Selectable("Conduction", g_modal_link_type == 0)) g_modal_link_type = 0;
            if (ImGui::Selectable("Convection", g_modal_link_type == 1)) g_modal_link_type = 1;
            if (ImGui::Selectable("Radiation", g_modal_link_type == 2)) g_modal_link_type = 2;
            if (ImGui::Selectable("Coolant Flow", g_modal_link_type == 3)) g_modal_link_type = 3;
            if (ImGui::Selectable("Fan / Pump Link", g_modal_link_type == 4)) g_modal_link_type = 4;
            ImGui::EndCombo();
        }
        
        std::string p1Label = "Conductance (W/K)";
        if (g_modal_link_type == 1) p1Label = "Convection coeff hA (W/K)";
        else if (g_modal_link_type == 2) p1Label = "Radiation parameter G_rad";
        else if (g_modal_link_type == 3) p1Label = "Fluid mass rate m_dot*cp (W/K)";
        else if (g_modal_link_type == 4) p1Label = "Shut-off Pressure P_max (Pa)";
        
        ImGui::InputDouble(p1Label.c_str(), &g_modal_link_p1, 1.0, 10.0, "%.2f");
        
        if (g_modal_link_type == 3) {
            int dir_idx = g_modal_link_p2 >= 0.0 ? 0 : 1;
            if (ImGui::BeginCombo("Fluid Flow Direction", dir_idx == 0 ? "Node A to Node B" : "Node B to Node A")) {
                if (ImGui::Selectable("Node A to Node B", dir_idx == 0)) g_modal_link_p2 = 1.0;
                if (ImGui::Selectable("Node B to Node A", dir_idx == 1)) g_modal_link_p2 = -1.0;
                ImGui::EndCombo();
            }
        } else if (g_modal_link_type == 4) {
            ImGui::InputDouble("Free Velocity v_max (m/s)", &g_modal_link_p2, 0.1, 1.0, "%.2f");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            g_show_link_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Link", ImVec2(120, 0))) {
            int nextId = 101;
            if (!g_links.empty()) {
                auto max_it = std::max_element(g_links.begin(), g_links.end(), [](const std::shared_ptr<DesktopLink>& a, const std::shared_ptr<DesktopLink>& b){
                    return a->id < b->id;
                });
                nextId = (*max_it)->id + 1;
            }
            double p1_val = g_modal_link_p1;
            double p2_val = g_modal_link_p2;
            if (g_modal_link_type == 4) {
                if (p1_val == 10.0) p1_val = 1000.0;
                if (p2_val == 1.0) p2_val = 10.0;
            }
            auto newLink = std::make_shared<DesktopLink>();
            newLink->id = nextId;
            newLink->node_a = g_modal_node_a_id;
            newLink->node_b = g_modal_node_b_id;
            newLink->type = g_modal_link_type;
            newLink->p1 = p1_val;
            newLink->p2 = p2_val;
            newLink->g_a1 = 0.0;
            newLink->g_a2 = 0.0;
            newLink->fan_area = 0.005;
            g_links.push_back(newLink);
            
            SyncSystemWithSolver();
            g_show_link_modal = false;
            g_selected_link = g_links.back();
            g_selected_node = nullptr;
            
            Log("Created connection link N:" + std::to_string(newLink->node_a) + " -> N:" + std::to_string(newLink->node_b), "info");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

