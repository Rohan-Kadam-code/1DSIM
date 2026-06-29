#include "app_state.h"
#include "../L2_domain/component_library.h"
#include "presets.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>

const std::vector<MaterialData> g_materials = {
    { "Custom", 0.0, 0.0, 0.0, 0.0 },
    { "Copper", 8960.0, 385.0, 0.1, 0.0 },
    { "Aluminium", 2700.0, 900.0, 0.5, 0.0 },
    { "Steel", 7850.0, 450.0, 0.2, 0.0 },
    { "Cast Iron", 7200.0, 460.0, 0.25, 0.0 },
    { "Silicon", 2330.0, 700.0, 0.3, 0.0 }
};

void UpdateNodeCapacityFromMaterial(const std::shared_ptr<DesktopNode>& node) {
    if (node->material == "Custom") return;
    for (const auto& mat : g_materials) {
        if (mat.name == node->material) {
            node->capacity = node->mass * mat.cp0;
            node->c_a1 = node->mass * mat.cp1;
            node->c_a2 = node->mass * mat.cp2;
            break;
        }
    }
}

void GetDesktopNodeProperties(const std::shared_ptr<DesktopNode>& node, double& cap, double& ca1, double& ca2) {
    if (node->domain == 1) { // Fluid
        double T_c = node->temp;
        double rho = 0.0, drho = 0.0, cp = 0.0, dcp = 0.0;
        std::string medium = node->fluid_medium;
        if (medium == "Water") {
            double T_clamp = std::max(0.0, std::min(T_c, 150.0));
            rho = 1000.0 - 0.0178 * T_clamp - 0.00557 * T_clamp * T_clamp + 0.000027 * T_clamp * T_clamp * T_clamp;
            drho = -0.0178 - 0.01114 * T_clamp + 0.000081 * T_clamp * T_clamp;
            cp = 4184.0 - 0.09 * T_clamp + 0.006 * T_clamp * T_clamp;
            dcp = -0.09 + 0.012 * T_clamp;
        } else if (medium == "Glycol") {
            double T_clamp = std::max(-40.0, std::min(T_c, 200.0));
            rho = 1060.0 - 0.65 * T_clamp;
            drho = -0.65;
            cp = 3300.0 + 3.5 * T_clamp;
            dcp = 3.5;
        } else if (medium == "Oil") {
            double T_clamp = std::max(-40.0, std::min(T_c, 250.0));
            rho = 890.0 - 0.60 * T_clamp;
            drho = -0.60;
            cp = 1900.0 + 3.0 * T_clamp;
            dcp = 3.0;
        } else if (medium == "Air") {
            double T_clamp = std::max(-150.0, std::min(T_c, 1000.0));
            double T_k = T_clamp + 273.15;
            rho = 101325.0 / (287.05 * T_k);
            drho = -101325.0 / (287.05 * T_k * T_k);
            cp = 1005.0;
            dcp = 0.0;
        } else if (medium == "Mixture") {
            double T_clamp = std::max(-40.0, std::min(T_c, 150.0));
            double w_rho = 1000.0 - 0.0178 * T_clamp - 0.00557 * T_clamp * T_clamp + 0.000027 * T_clamp * T_clamp * T_clamp;
            double g_rho = 1060.0 - 0.65 * T_clamp;
            double f = node->fluid_mix_ratio;
            rho = (1.0 - f) * w_rho + f * g_rho;
            drho = (1.0 - f) * (-0.0178 - 0.01114 * T_clamp + 0.000081 * T_clamp * T_clamp) + f * (-0.65);
            double w_cp = 4184.0 - 0.09 * T_clamp + 0.006 * T_clamp * T_clamp;
            double g_cp = 3300.0 + 3.5 * T_clamp;
            cp = (1.0 - f) * w_cp + f * g_cp;
            dcp = (1.0 - f) * (-0.09 + 0.012 * T_clamp) + f * 3.5;
        } else {
            double T_clamp = std::max(-273.0, T_c);
            double T_k = T_clamp + 273.15;
            rho = node->fluid_rho_a0 + node->fluid_rho_a1 * T_k + node->fluid_rho_a2 * T_k * T_k;
            drho = node->fluid_rho_a1 + 2.0 * node->fluid_rho_a2 * T_k;
            cp = node->fluid_cp_a0 + node->fluid_cp_a1 * T_k + node->fluid_cp_a2 * T_k * T_k;
            dcp = node->fluid_cp_a1 + 2.0 * node->fluid_cp_a2 * T_k;
        }
        
        rho = std::max(0.01, rho);
        cp = std::max(10.0, cp);
        
        double vol_m3 = (node->fluid_volume) * 0.001; // L to m^3
        cap = rho * vol_m3 * cp;
        ca1 = vol_m3 * (rho * dcp + cp * drho);
        ca2 = 0.0;
    } else {
        cap = node->capacity;
        ca1 = node->c_a1;
        ca2 = node->c_a2;
    }
}

// Instantiate all global variables
std::vector<std::shared_ptr<DesktopNode>> g_nodes;
std::vector<std::shared_ptr<DesktopLink>> g_links;
ThermalSystem            g_solver;
std::shared_ptr<DesktopNode> g_selected_node = nullptr;
std::shared_ptr<DesktopLink> g_selected_link = nullptr;
std::string              g_active_preset = "vehicle";

std::vector<ModelState> g_undo_stack;
std::vector<ModelState> g_redo_stack;
ModelState              g_drag_backup;
bool                    g_drag_backup_valid = false;

bool                     g_is_running = false;
double                   g_sim_time = 0.0;
double                   g_time_step = 0.05;
std::string              g_solver_type = "rk4";
float                    g_implicit_tolerance = 1e-5f;
int                      g_implicit_max_iter = 50;
int                      g_sim_speed = 1;
bool                     g_grid_snap = true;
const int                GRID_SIZE = 20;

std::vector<double>      g_time_history;
std::unordered_map<int, std::vector<double>> g_temp_history;
std::unordered_map<int, bool> g_plot_active_nodes;

std::vector<LogLine>     g_logs;
std::vector<SliderConfig> g_sliders;

ImVec2                   g_canvas_scrolling = ImVec2(0.0f, 0.0f);
float                    g_canvas_zoom = 1.0f;
int                      g_current_tool = 0;
std::shared_ptr<DesktopNode> g_linking_start_node = nullptr;
int                      g_pending_link_type = 0;
ImVec2                   g_temp_mouse_pos;

bool                     isDragging = false;
std::shared_ptr<DesktopNode> dragNode = nullptr;
ImVec2                   dragOffset;

bool                     g_show_link_modal = false;
int                      g_modal_link_type = 0;
double                   g_modal_link_p1 = 10.0;
double                   g_modal_link_p2 = 1.0;
int                      g_modal_node_a_id = -1;
int                      g_modal_node_b_id = -1;

std::string   g_pending_comp_type = "";
bool          g_placing_component = false;

int           g_conn_from_inst = -1;
std::string   g_conn_from_port = "";
std::string   g_hovered_port_id = "";
int           g_hovered_port_inst = -1;

int           g_lib_tab = 0;
int           g_next_inst_id = 1000;
int           g_next_conn_id = 2000;

bool          g_comp_mode = false;
bool          g_force_tab_generic = false;
bool          g_force_tab_component = false;
bool          g_reset_dockspace = false;

std::vector<CompInstance>    g_comp_instances;
std::vector<CompConnection>  g_comp_connections;
CompInstance*                g_sel_comp = nullptr;
CompConnection*              g_sel_conn = nullptr;

// Shared Helper functions
void ResolvePointers() {
    if (g_selected_node != nullptr) {
        int targetId = g_selected_node->id;
        std::shared_ptr<DesktopNode> foundNode = nullptr;
        for (auto& n : g_nodes) {
            if (n->id == targetId) {
                foundNode = n;
                break;
            }
        }
        g_selected_node = foundNode;
    }
    if (dragNode != nullptr) {
        int targetId = dragNode->id;
        std::shared_ptr<DesktopNode> foundNode = nullptr;
        for (auto& n : g_nodes) {
            if (n->id == targetId) {
                foundNode = n;
                break;
            }
        }
        dragNode = foundNode;
    }
    if (g_linking_start_node != nullptr) {
        int targetId = g_linking_start_node->id;
        std::shared_ptr<DesktopNode> foundNode = nullptr;
        for (auto& n : g_nodes) {
            if (n->id == targetId) {
                foundNode = n;
                break;
            }
        }
        g_linking_start_node = foundNode;
    }
    if (g_selected_link != nullptr) {
        int targetId = g_selected_link->id;
        std::shared_ptr<DesktopLink> foundLink = nullptr;
        for (auto& l : g_links) {
            if (l->id == targetId) {
                foundLink = l;
                break;
            }
        }
        g_selected_link = foundLink;
    }
}

void SyncSelection() {
    ResolvePointers();
}

void SyncComponentsWithSolver() {
    ResolvePointers();
    SyncSystemWithSolver();
    ResetHistory();
}

void SyncSystemWithSolver() {
    g_solver.clear();
    for (const auto& top_n : g_nodes) {
        for (auto* n : top_n->GetSolverNodes()) {
            g_solver.add_node(n->id, n->name.c_str(), cToK(n->temp), n->capacity, n->q_gen, n->is_fixed, n->c_a1, n->c_a2, n->domain, n->fluid_medium.c_str(), n->fluid_volume);
            if (n->domain == 1) {
                g_solver.set_node_fluid_params(n->id, n->fluid_mix_ratio, n->fluid_rho_a0, n->fluid_rho_a1, n->fluid_rho_a2, n->fluid_cp_a0, n->fluid_cp_a1, n->fluid_cp_a2);
            }
        }
    }
    for (const auto& l : g_links) {
        auto itA = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == l->node_a; });
        auto itB = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == l->node_b; });
        int nodeA_resolved = (itA != g_nodes.end()) ? (*itA)->ResolveSolverNodeId(l->type, true) : l->node_a;
        int nodeB_resolved = (itB != g_nodes.end()) ? (*itB)->ResolveSolverNodeId(l->type, false) : l->node_b;
        g_solver.add_link(l->id, nodeA_resolved, nodeB_resolved, l->type, l->p1, l->p2, l->g_a1, l->g_a2, l->fan_area);
    }
    for (const auto& top_n : g_nodes) {
        for (const auto& l : top_n->GetInternalLinks()) {
            g_solver.add_link(l->id, l->node_a, l->node_b, l->type, l->p1, l->p2, l->g_a1, l->g_a2, l->fan_area);
        }
    }
}

void ResetHistory() {
    g_time_history.clear();
    g_temp_history.clear();
    
    g_time_history.push_back(g_sim_time);
    for (const auto& n : g_nodes) {
        g_temp_history[n->id].push_back(n->temp);
    }
}

void Log(const std::string& message, const std::string& type) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    
    // Use local time securely
    struct tm buf;
    localtime_s(&buf, &in_time_t);
    ss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S");
    
    LogLine line;
    line.time = ss.str().substr(11, 8); // HH:MM:SS
    line.message = message;
    line.type = type;
    g_logs.push_back(line);
}

void PushUndoState() {
    ModelState state;
    state.nodes = g_nodes;
    state.links = g_links;
    g_undo_stack.push_back(state);
    if (g_undo_stack.size() > 50) {
        g_undo_stack.erase(g_undo_stack.begin());
    }
    g_redo_stack.clear();
}

void Undo() {
    if (g_undo_stack.empty()) return;
    ModelState current;
    current.nodes = g_nodes;
    current.links = g_links;
    g_redo_stack.push_back(current);
    
    ModelState prev = g_undo_stack.back();
    g_undo_stack.pop_back();
    
    g_nodes = prev.nodes;
    g_links = prev.links;
    
    g_selected_node = nullptr;
    g_selected_link = nullptr;
    
    SyncSystemWithSolver();
    SyncSlidersFromSystem();
    ResetHistory();
    Log("Undo action performed.", "info");
}

void Redo() {
    if (g_redo_stack.empty()) return;
    ModelState current;
    current.nodes = g_nodes;
    current.links = g_links;
    g_undo_stack.push_back(current);
    
    ModelState next = g_redo_stack.back();
    g_redo_stack.pop_back();
    
    g_nodes = next.nodes;
    g_links = next.links;
    
    g_selected_node = nullptr;
    g_selected_link = nullptr;
    
    SyncSystemWithSolver();
    SyncSlidersFromSystem();
    ResetHistory();
    Log("Redo action performed.", "info");
}

void SyncSlidersFromSystem() {
    for (auto& slide : g_sliders) {
        if (slide.target_type == "node") {
            auto it = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == slide.target_id; });
            if (it != g_nodes.end()) {
                if (slide.field == "temp") {
                    slide.value = (float)(*it)->temp;
                }
                else if (slide.field == "q_gen") {
                    slide.value = (float)(*it)->q_gen;
                }
            }
        }
        else if (slide.target_type == "node_kw") {
            auto it = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == slide.target_id; });
            if (it != g_nodes.end()) {
                if (slide.field == "q_gen") {
                    slide.value = (float)((*it)->q_gen / 1000.0);
                }
            }
        }
        else if (slide.target_type == "nodes_all_q") {
            if (!slide.target_ids.empty()) {
                auto it = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == slide.target_ids[0]; });
                if (it != g_nodes.end()) {
                    slide.value = (float)(*it)->q_gen;
                }
            }
        }
        else if (slide.target_type == "link") {
            auto it = std::find_if(g_links.begin(), g_links.end(), [&](const std::shared_ptr<DesktopLink>& l) { return l->id == slide.target_id; });
            if (it != g_links.end()) {
                if (slide.field == "p1") {
                    slide.value = (float)(*it)->p1;
                }
            }
        }
        else if (slide.target_type == "links_all_flow") {
            if (!slide.target_ids.empty()) {
                auto it = std::find_if(g_links.begin(), g_links.end(), [&](const std::shared_ptr<DesktopLink>& l) { return l->id == slide.target_ids[0]; });
                if (it != g_links.end()) {
                    slide.value = (float)(*it)->p1;
                }
            }
        }
    }
}

void ApplySlidersToSystem() {
    for (const auto& slide : g_sliders) {
        if (slide.target_type == "node") {
            auto it = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == slide.target_id; });
            if (it != g_nodes.end()) {
                if (slide.field == "temp") {
                    (*it)->temp = slide.value;
                    (*it)->temp_init = slide.value;
                }
                else if (slide.field == "q_gen") {
                    (*it)->q_gen = slide.value;
                }
            }
        }
        else if (slide.target_type == "node_kw") {
            auto it = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == slide.target_id; });
            if (it != g_nodes.end()) {
                if (slide.field == "q_gen") {
                    (*it)->q_gen = slide.value * 1000.0; // kW to W
                }
            }
        }
        else if (slide.target_type == "nodes_all_q") {
            for (int tid : slide.target_ids) {
                auto it = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == tid; });
                if (it != g_nodes.end()) {
                    (*it)->q_gen = slide.value;
                }
            }
        }
        else if (slide.target_type == "link") {
            auto it = std::find_if(g_links.begin(), g_links.end(), [&](const std::shared_ptr<DesktopLink>& l) { return l->id == slide.target_id; });
            if (it != g_links.end()) {
                if (slide.field == "p1") {
                    (*it)->p1 = slide.value;
                }
            }
        }
        else if (slide.target_type == "links_all_flow") {
            for (int tid : slide.target_ids) {
                auto it = std::find_if(g_links.begin(), g_links.end(), [&](const std::shared_ptr<DesktopLink>& l) { return l->id == tid; });
                if (it != g_links.end()) {
                    (*it)->p1 = slide.value;
                }
            }
        }
    }
}

void PlaceComponent(const std::string& defId, float cx, float cy) {
    PushUndoState();
    if (defId == "engine_block") {
        auto node = std::make_shared<EngineBlockNode>();
        node->id = g_nodes.empty() ? 100 : (*std::max_element(g_nodes.begin(), g_nodes.end(), [](auto& a, auto& b){ return a->id < b->id; }))->id + 100;
        node->x = cx; node->y = cy;
        node->jacketNode->id = node->id + 1;
        node->internalCond->id = node->id + 2;
        node->internalCond->node_a = node->id;
        node->internalCond->node_b = node->jacketNode->id;
        node->capacity = node->params["block_capacity"];
        node->q_gen = node->params["heat_rejection"];
        node->jacketNode->fluid_volume = node->params["jacket_volume"];
        node->internalCond->p1 = node->params["block_jacket_cond"];
        g_nodes.push_back(node);
    } else if (defId == "radiator") {
        auto node = std::make_shared<RadiatorNode>();
        node->id = g_nodes.empty() ? 100 : (*std::max_element(g_nodes.begin(), g_nodes.end(), [](auto& a, auto& b){ return a->id < b->id; }))->id + 100;
        node->x = cx; node->y = cy;
        node->coreNode->id = node->id + 1;
        node->internalConv->id = node->id + 2;
        node->internalConv->node_a = node->id;
        node->internalConv->node_b = node->coreNode->id;
        node->fluid_volume = node->params["coolant_volume"];
        node->coreNode->capacity = node->params["core_capacity"];
        node->internalConv->p1 = node->params["coolant_hA"];
        g_nodes.push_back(node);
    } else if (defId == "ambient_air") {
        auto node = std::make_shared<AmbientAirNode>();
        node->id = g_nodes.empty() ? 100 : (*std::max_element(g_nodes.begin(), g_nodes.end(), [](auto& a, auto& b){ return a->id < b->id; }))->id + 100;
        node->x = cx; node->y = cy;
        node->temp = node->params["temp_c"];
        g_nodes.push_back(node);
    } else if (defId == "generic_mass") {
        auto node = std::make_shared<DesktopNode>();
        node->id = g_nodes.empty() ? 100 : (*std::max_element(g_nodes.begin(), g_nodes.end(), [](auto& a, auto& b){ return a->id < b->id; }))->id + 100;
        node->x = cx; node->y = cy;
        node->name = "Mass";
        node->capacity = 1000.0;
        node->is_fixed = false;
        g_nodes.push_back(node);
        Log("Placed Thermal Mass node", "info");
    } else if (defId == "generic_boundary") {
        auto node = std::make_shared<DesktopNode>();
        node->id = g_nodes.empty() ? 100 : (*std::max_element(g_nodes.begin(), g_nodes.end(), [](auto& a, auto& b){ return a->id < b->id; }))->id + 100;
        node->x = cx; node->y = cy;
        node->name = "Boundary";
        node->temp = 25.0;
        node->is_fixed = true;
        g_nodes.push_back(node);
        Log("Placed Fixed Boundary node", "info");
    } else {
        const ComponentDef* def = GetCompDefById(defId);
        if (def) {
            auto node = std::make_shared<DesktopNode>();
            node->id = g_nodes.empty() ? 100 : (*std::max_element(g_nodes.begin(), g_nodes.end(), [](auto& a, auto& b){ return a->id < b->id; }))->id + 100;
            node->x = cx; node->y = cy;
            node->name = def->name;
            g_nodes.push_back(node);
            Log("Placed generic wrapper for " + def->name, "info");
        }
    }
    SyncComponentsWithSolver();
}

void UpdateHistory() {
    g_time_history.push_back(g_sim_time);
    for (const auto& n : g_nodes) {
        g_temp_history[n->id].push_back(n->temp);
    }
    
    // Cap plot points
    const size_t max_pts = 600;
    if (g_time_history.size() > max_pts) {
        g_time_history.erase(g_time_history.begin());
        for (auto& pair : g_temp_history) {
            pair.second.erase(pair.second.begin());
        }
    }
}

void StepSimulation() {
    // Dynamic updates: apply slider values to nodes and links
    ApplySlidersToSystem();

    // Update thermostat bypass split flow rates
    UpdateThermostatBypassFlows();

    // Load state to solver
    SyncSystemWithSolver();

    // Perform numerical integration step
    if (g_solver_type == "rk4") {
        g_solver.step_rk4(g_time_step);
    } else if (g_solver_type == "backward_euler") {
        g_solver.step_backward_euler(g_time_step, g_implicit_tolerance, g_implicit_max_iter);
    } else {
        g_solver.step_explicit_euler(g_time_step);
    }

    g_sim_time += g_time_step;

    // Pull temperatures back
    for (auto& n : g_nodes) {
        n->temp = kToC(g_solver.get_node_temperature(n->id));
    }

    UpdateHistory();
}

void SolveSteadyState() {
    int total_iters = 0;
    // Iterate to converge temperature-dependent thermostat valve split
    for (int iter = 0; iter < 10; ++iter) {
        ApplySlidersToSystem();
        UpdateThermostatBypassFlows();
        SyncSystemWithSolver();
        total_iters += g_solver.solve_steady_state(1e-6, 500);
        
        // Pull temperatures back
        for (auto& n : g_nodes) {
            n->temp = kToC(g_solver.get_node_temperature(n->id));
        }
    }
    
    UpdateHistory();
    Log("Steady State Solver converged in " + std::to_string(total_iters) + " total sweeps.", "success");
}

void ExportCSV() {
    if (g_time_history.empty()) {
        Log("No simulation history available to export.", "warning");
        return;
    }
    
    std::ofstream out("thermal_sim_export.csv");
    if (!out.is_open()) {
        Log("Failed to create export file.", "error");
        return;
    }
    
    // Header
    out << "Time (s)";
    for (const auto& n : g_nodes) {
        out << "," << n->name << " (C)";
    }
    out << "\n";
    
    // Rows
    for (size_t i = 0; i < g_time_history.size(); i++) {
        out << g_time_history[i];
        for (const auto& n : g_nodes) {
            auto& temps = g_temp_history[n->id];
            if (i < temps.size()) {
                out << "," << temps[i];
            } else {
                out << ",";
            }
        }
        out << "\n";
    }
    out.close();
    Log("Simulation history exported successfully to 'thermal_sim_export.csv'.", "success");
}

ImVec2 CanvasToScreen(ImVec2 local_pos, ImVec2 canvas_origin) {
    return ImVec2(
        canvas_origin.x + (local_pos.x + g_canvas_scrolling.x) * g_canvas_zoom,
        canvas_origin.y + (local_pos.y + g_canvas_scrolling.y) * g_canvas_zoom
    );
}

ImVec2 ScreenToCanvas(ImVec2 screen_pos, ImVec2 canvas_origin) {
    return ImVec2(
        (screen_pos.x - canvas_origin.x) / g_canvas_zoom - g_canvas_scrolling.x,
        (screen_pos.y - canvas_origin.y) / g_canvas_zoom - g_canvas_scrolling.y
    );
}

void clearWorkspace() {
    PushUndoState(); // Save state before clearing
    g_nodes.clear();
    g_links.clear();
    g_selected_node = nullptr;
    g_selected_link = nullptr;
    g_sim_time = 0.0;
    g_time_history.clear();
    g_temp_history.clear();
    g_canvas_zoom = 1.0f;
    g_canvas_scrolling = ImVec2(0.0f, 0.0f);
    SyncSystemWithSolver();
    Log("Workspace cleared.", "warning");
}

void resetSimulation() {
    g_is_running = false;
    g_sim_time = 0.0;
    
    // Reset temperatures back to initial user configuration
    for (auto& n : g_nodes) {
        n->temp = n->temp_init;
    }
    
    SyncSystemWithSolver();
    ResetHistory();
    Log("Simulation state reset.");
}

void deleteSelected() {
    if (g_selected_node || g_selected_link) {
        PushUndoState(); // Save state before deleting
    }
    if (g_selected_node) {
        int id = g_selected_node->id;
        // Erase connected links first
        g_links.erase(std::remove_if(g_links.begin(), g_links.end(), [&](const std::shared_ptr<DesktopLink>& l) {
            return l->node_a == id || l->node_b == id;
        }), g_links.end());
        // Erase node
        g_nodes.erase(std::remove_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) {
            return n->id == id;
        }), g_nodes.end());
        
        Log("Deleted node component " + std::to_string(id), "warning");
        g_selected_node = nullptr;
    } 
    else if (g_selected_link) {
        int id = g_selected_link->id;
        g_links.erase(std::remove_if(g_links.begin(), g_links.end(), [&](const std::shared_ptr<DesktopLink>& l) {
            return l->id == id;
        }), g_links.end());
        
        Log("Deleted connection link " + std::to_string(id), "warning");
        g_selected_link = nullptr;
    }
    SyncSystemWithSolver();
    ResetHistory();
}

void setTool(int tool) {
    g_current_tool = tool;
    g_linking_start_node = nullptr;
}
