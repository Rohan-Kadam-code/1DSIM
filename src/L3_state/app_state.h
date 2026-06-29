#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include "../L1_physics/solver.h"
#include "../L2_domain/sim_elements.h"
#include "../L2_domain/component_library.h"
#include "../../thirdparty/imgui/imgui.h"

// Forward declarations of UI actions & helpers
void clearWorkspace();
void resetSimulation();
void deleteSelected();
void setTool(int tool);
void PushUndoState();
void Undo();
void Redo();
void SyncSlidersFromSystem();
void ApplySlidersToSystem();
void SaveModel();
void LoadModel();

void SyncSystemWithSolver();
void ResetHistory();
void Log(const std::string& message, const std::string& type = "info");
ImVec2 CanvasToScreen(ImVec2 local_pos, ImVec2 canvas_origin);
ImVec2 ScreenToCanvas(ImVec2 screen_pos, ImVec2 canvas_origin);

std::shared_ptr<DesktopNode> getNodeAt(ImVec2 pos);
std::shared_ptr<DesktopLink> getLinkAt(ImVec2 pos);
void ResolvePointers();
void SyncSelection();
void SyncComponentsWithSolver();

const double K_ZERO = 273.15;
inline double cToK(double c) { return c + K_ZERO; }
inline double kToC(double k) { return k - K_ZERO; }

struct ModelState {
    std::vector<std::shared_ptr<DesktopNode>> nodes;
    std::vector<std::shared_ptr<DesktopLink>> links;

    ModelState Clone() const {
        ModelState copy;
        for (auto& n : nodes) copy.nodes.push_back(n->clone());
        for (auto& l : links) copy.links.push_back(l->clone());
        return copy;
    }
};

struct SliderConfig {
    std::string id;
    std::string label;
    float min_val;
    float max_val;
    float value;
    float step;
    std::string target_type; // "node", "node_kw", "nodes_all_q", "link", "links_all_flow"
    int target_id;
    std::vector<int> target_ids;
    std::string field;       // "temp", "q_gen", "p1"
};

struct LogLine {
    std::string time;
    std::string message;
    std::string type; // "info", "success", "warning", "error"
};

struct MaterialData {
    std::string name;
    double density;   // kg/m^3
    double cp0;       // J/(kg*K)
    double cp1;       // J/(kg*K^2)
    double cp2;       // J/(kg*K^3)
};

extern const std::vector<MaterialData> g_materials;
void UpdateNodeCapacityFromMaterial(const std::shared_ptr<DesktopNode>& node);
void GetDesktopNodeProperties(const std::shared_ptr<DesktopNode>& node, double& cap, double& ca1, double& ca2);

// Global application state
extern std::vector<std::shared_ptr<DesktopNode>> g_nodes;
extern std::vector<std::shared_ptr<DesktopLink>> g_links;
extern ThermalSystem            g_solver;
extern std::shared_ptr<DesktopNode> g_selected_node;
extern std::shared_ptr<DesktopLink> g_selected_link;
extern std::string              g_active_preset;

extern std::vector<ModelState> g_undo_stack;
extern std::vector<ModelState> g_redo_stack;
extern ModelState              g_drag_backup;
extern bool                    g_drag_backup_valid;

extern bool                     g_is_running;
extern double                   g_sim_time;
extern double                   g_time_step;
extern std::string              g_solver_type;
extern float                    g_implicit_tolerance;
extern int                      g_implicit_max_iter;
extern int                      g_sim_speed;
extern bool                     g_grid_snap;
extern const int                GRID_SIZE;

// Time history for plotting
extern std::vector<double>      g_time_history;
extern std::unordered_map<int, std::vector<double>> g_temp_history;
extern std::unordered_map<int, bool> g_plot_active_nodes; // nodeId -> visible

// Log Console
extern std::vector<LogLine>     g_logs;

// Slider states
extern std::vector<SliderConfig> g_sliders;

// Canvas state
extern ImVec2                   g_canvas_scrolling;
extern float                    g_canvas_zoom;
extern int                      g_current_tool; // 0: Select, 1: Add Link
extern std::shared_ptr<DesktopNode> g_linking_start_node;
extern int                      g_pending_link_type; // 0:Cond, 1:Conv, 2:Rad, 3:Flow, 4:Fan
extern ImVec2                   g_temp_mouse_pos;

extern bool                     isDragging;
extern std::shared_ptr<DesktopNode> dragNode;
extern ImVec2                   dragOffset;

// Modal creation helper
extern bool                     g_show_link_modal;
extern int                      g_modal_link_type;
extern double                   g_modal_link_p1;
extern double                   g_modal_link_p2;
extern int                      g_modal_node_a_id;
extern int                      g_modal_node_b_id;

// Component canvas tool state (tool 2 = Place Component)
extern std::string   g_pending_comp_type;   // defId being dragged from palette
extern bool          g_placing_component;  // cursor shows ghost

// Port connection state (tool 3 = Connect Ports)
extern int           g_conn_from_inst;
extern std::string   g_conn_from_port;
extern std::string   g_hovered_port_id;
extern int           g_hovered_port_inst;

// Library panel category
extern int           g_lib_tab;   // 0=ICE

// Running instance ID counter
extern int           g_next_inst_id;
extern int           g_next_conn_id;

// Component mode flag (defaults to true for Physical Library, but loading preset toggles generic)
extern bool          g_comp_mode;
extern bool          g_force_tab_generic;
extern bool          g_force_tab_component;

extern bool          g_reset_dockspace;
extern std::vector<CompInstance>    g_comp_instances;   // placed components
extern std::vector<CompConnection>  g_comp_connections; // wired connections
extern CompInstance*                g_sel_comp;         // selected component
extern CompConnection*              g_sel_conn;         // selected connection

void UpdateHistory();
void PlaceComponent(const std::string& defId, float cx, float cy);
void StepSimulation();
void SolveSteadyState();
void ExportCSV();
