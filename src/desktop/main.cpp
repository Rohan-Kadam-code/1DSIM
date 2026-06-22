#define NOMINMAX
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <d3d11.h>
#include <tchar.h>
#include <commdlg.h>

#include "../core/solver.h"
#include "sim_elements.h"
#include "component_library.h"
#include "../../thirdparty/imgui/imgui.h"
#include "../../thirdparty/imgui/imgui_internal.h"
#include "../../thirdparty/imgui/backends/imgui_impl_win32.h"
#include "../../thirdparty/imgui/backends/imgui_impl_dx11.h"
#include "../../thirdparty/implot/implot.h"
#include "../../thirdparty/json.hpp"

// Forward declarations of UI actions & helpers
struct DesktopNode;
struct PortPos {
    char name;
    ImVec2 pos;
};
std::vector<PortPos> GetPortPositions(std::shared_ptr<DesktopNode> node);
PortPos GetClosestPort(std::shared_ptr<DesktopNode> nodeThis, std::shared_ptr<DesktopNode> nodeOther);
std::vector<ImVec2> ComputeOrthogonalRoute(ImVec2 portA, char faceA, ImVec2 portB, char faceB);

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

// --- DX11 Global Variables ---
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Helper functions for DX11
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- SIMULATION DATA STRUCTURES FOR DESKTOP ---
// DesktopNode and DesktopLink moved to sim_elements.h


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

// Global application state
static std::vector<std::shared_ptr<DesktopNode>> g_nodes;
static std::vector<std::shared_ptr<DesktopLink>> g_links;
static ThermalSystem            g_solver;
static std::shared_ptr<DesktopNode> g_selected_node = nullptr;
static std::shared_ptr<DesktopLink> g_selected_link = nullptr;
static std::string              g_active_preset = "vehicle";

static std::vector<ModelState> g_undo_stack;
static std::vector<ModelState> g_redo_stack;
static ModelState              g_drag_backup;
static bool                    g_drag_backup_valid = false;

static bool                     g_is_running = false;
static double                   g_sim_time = 0.0;
static double                   g_time_step = 0.05;
static std::string              g_solver_type = "rk4";
static float                    g_implicit_tolerance = 1e-5f;
static int                      g_implicit_max_iter = 50;
static int                      g_sim_speed = 1;
static bool                     g_grid_snap = true;
static const int                GRID_SIZE = 20;

// Time history for plotting
static std::vector<double>      g_time_history;
static std::unordered_map<int, std::vector<double>> g_temp_history;
static std::unordered_map<int, bool> g_plot_active_nodes; // nodeId -> visible

// Log Console
static std::vector<LogLine>     g_logs;

// Slider states
static std::vector<SliderConfig> g_sliders;

// Canvas state
static ImVec2                   g_canvas_scrolling = ImVec2(0.0f, 0.0f);
static float                    g_canvas_zoom = 1.0f;
static int                      g_current_tool = 0; // 0: Select, 1: Add Link
static std::shared_ptr<DesktopNode> g_linking_start_node = nullptr;
static int                      g_pending_link_type = 0; // 0:Cond, 1:Conv, 2:Rad, 3:Flow, 4:Fan
static ImVec2                   g_temp_mouse_pos;

static bool                     isDragging = false;
static std::shared_ptr<DesktopNode> dragNode = nullptr;
static ImVec2                   dragOffset;

// Modal creation helper
static bool                     g_show_link_modal = false;
static int                      g_modal_link_type = 0;
static double                   g_modal_link_p1 = 10.0;
static double                   g_modal_link_p2 = 1.0;
static int                      g_modal_node_a_id = -1;
static int                      g_modal_node_b_id = -1;

// ─── COMPONENT LIBRARY STATE ─────────────────────────────────────────────────
// Legacy globals removed

// Component canvas tool state (tool 2 = Place Component)
static std::string   g_pending_comp_type = "";   // defId being dragged from palette
static bool          g_placing_component = false;  // cursor shows ghost

// Port connection state (tool 3 = Connect Ports)
static int           g_conn_from_inst = -1;
static std::string   g_conn_from_port = "";
static std::string   g_hovered_port_id = "";
static int           g_hovered_port_inst = -1;

// Library panel category
static int           g_lib_tab = 0;   // 0=ICE

// Running instance ID counter
static int           g_next_inst_id = 1000;
static int           g_next_conn_id = 2000;

// Component mode flag (defaults to true for Physical Library, but loading preset toggles generic)
static bool g_comp_mode = false;
static bool          g_force_tab_generic = false;
static bool          g_force_tab_component = false;

// Helper: get def for a placed instance

static bool g_reset_dockspace = false;

// Safely resolve selection and drag pointers after vectors are modified/reallocated

static void ResolvePointers() {
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

// Synchronise selection between components and generic nodes

static void SyncSelection() {
    ResolvePointers();
}

// Compile component graph and sync solver

static void SyncComponentsWithSolver() {
    ResolvePointers();
    SyncSystemWithSolver();
    ResetHistory();
}

// Place a component instance at canvas position

static void PlaceComponent(const std::string& defId, float cx, float cy) {
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

// Draw all component instances on the canvas
static void DrawComponentInstances(ImDrawList* dl, ImVec2 canvas_origin, float zoom) {}

// Draw component connections
static void DrawComponentConnections(ImDrawList* dl, ImVec2 canvas_origin, float zoom) {}

// Canvas coordinate helper functions
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

// Canvas Node/Link Hit helpers
std::shared_ptr<DesktopNode> getNodeAt(ImVec2 pos) {
    for (auto& n : g_nodes) {
        float w, h;
        n->GetBounds(w, h);
        if (pos.x >= n->x - w / 2.0f && pos.x <= n->x + w / 2.0f &&
            pos.y >= n->y - h / 2.0f && pos.y <= n->y + h / 2.0f) {
            return n;
        }
    }
    return nullptr;
}

float getDistanceToSegment(ImVec2 p, ImVec2 a, ImVec2 b) {
    float l2 = std::pow(a.x - b.x, 2.0f) + std::pow(a.y - b.y, 2.0f);
    if (l2 == 0.0f) return std::hypot(p.x - a.x, p.y - a.y);
    float t = ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2;
    t = std::max(0.0f, std::min(1.0f, t));
    return std::hypot(p.x - (a.x + t * (b.x - a.x)), p.y - (a.y + t * (b.y - a.y)));
}

std::shared_ptr<DesktopLink> getLinkAt(ImVec2 pos) {
    for (auto& l : g_links) {
        auto itA = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == l->node_a; });
        auto itB = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == l->node_b; });
        if (itA == g_nodes.end() || itB == g_nodes.end()) continue;
        
        PortPos portA = GetClosestPort(*itA, *itB);
        PortPos portB = GetClosestPort(*itB, *itA);
        
        std::vector<ImVec2> route = ComputeOrthogonalRoute(portA.pos, portA.name, portB.pos, portB.name);
        
        float minDist = 1e9f;
        for (size_t i = 0; i < route.size() - 1; ++i) {
            float dist = getDistanceToSegment(pos, route[i], route[i+1]);
            if (dist < minDist) {
                minDist = dist;
            }
        }
        
        if (minDist <= 10.0f / g_canvas_zoom) {
            return l;
        }
    }
    return nullptr;
}

// Celsius / Kelvin helper
const double K_ZERO = 273.15;
inline double cToK(double c) { return c + K_ZERO; }
inline double kToC(double k) { return k - K_ZERO; }

// Logging helper
void Log(const std::string& message, const std::string& type) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    
    LogLine line;
    line.time = ss.str().substr(11, 8); // Keep HH:MM:SS
    line.message = message;
    line.type = type;
    g_logs.push_back(line);
}

// --- MATERIAL DATABASE ---
struct MaterialData {
    std::string name;
    double density;   // kg/m^3
    double cp0;       // J/(kg*K)
    double cp1;       // J/(kg*K^2)
    double cp2;       // J/(kg*K^3)
};

static const std::vector<MaterialData> g_materials = {
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
            rho = 1000.0 - 0.0178 * T_c - 0.00557 * T_c * T_c + 0.000027 * T_c * T_c * T_c;
            drho = -0.0178 - 0.01114 * T_c + 0.000081 * T_c * T_c;
            cp = 4184.0 - 0.09 * T_c + 0.006 * T_c * T_c;
            dcp = -0.09 + 0.012 * T_c;
        } else if (medium == "Glycol") {
            rho = 1060.0 - 0.65 * T_c;
            drho = -0.65;
            cp = 3300.0 + 3.5 * T_c;
            dcp = 3.5;
        } else if (medium == "Oil") {
            rho = 890.0 - 0.60 * T_c;
            drho = -0.60;
            cp = 1800.0 + 3.0 * T_c;
            dcp = 3.0;
        } else if (medium == "Air") {
            double denom = T_c + 273.15;
            if (denom < 10.0) denom = 10.0;
            rho = 353.18295 / denom;
            drho = -353.18295 / (denom * denom);
            cp = 1005.0 + 0.05 * T_c;
            dcp = 0.05;
        } else if (medium == "Mixture") {
            double r = node->fluid_mix_ratio;
            double ir = 1.0 - r;
            // Water properties
            double rho_w = 1000.0 - 0.0178 * T_c - 0.00557 * T_c * T_c + 0.000027 * T_c * T_c * T_c;
            double drho_w = -0.0178 - 0.01114 * T_c + 0.000081 * T_c * T_c;
            double cp_w = 4184.0 - 0.09 * T_c + 0.006 * T_c * T_c;
            double dcp_w = -0.09 + 0.012 * T_c;
            
            // Pure Glycol properties
            double rho_g = 1060.0 - 0.65 * T_c;
            double drho_g = -0.65;
            double cp_g = 3300.0 + 3.5 * T_c;
            double dcp_g = 3.5;
            
            rho = ir * rho_w + r * rho_g;
            drho = ir * drho_w + r * drho_g;
            cp = ir * cp_w + r * cp_g;
            dcp = ir * dcp_w + r * dcp_g;
        } else if (medium == "Custom") {
            rho = node->fluid_rho_a0 + node->fluid_rho_a1 * T_c + node->fluid_rho_a2 * T_c * T_c;
            drho = node->fluid_rho_a1 + 2.0 * node->fluid_rho_a2 * T_c;
            cp = node->fluid_cp_a0 + node->fluid_cp_a1 * T_c + node->fluid_cp_a2 * T_c * T_c;
            dcp = node->fluid_cp_a1 + 2.0 * node->fluid_cp_a2 * T_c;
        } else {
            rho = 1000.0; drho = 0.0; cp = 4184.0; dcp = 0.0;
        }
        double V_m3 = node->fluid_volume * 1e-3;
        cap = rho * V_m3 * cp;
        ca1 = V_m3 * (rho * dcp + cp * drho);
        ca2 = 0.0;
    } else { // Solid
        cap = node->capacity;
        ca1 = node->c_a1;
        ca2 = node->c_a2;
    }
}

// --- PORT CALCULATIONS ---

std::vector<PortPos> GetPortPositions(std::shared_ptr<DesktopNode> node) {
    std::vector<PortPos> ports;
    auto nodePorts = node->GetPorts();
    if (!nodePorts.empty()) {
        for (const auto& p : nodePorts) {
            ports.push_back({ p.face, ImVec2(node->x + p.dx, node->y + p.dy) });
        }
    } else {
        const float w = 70.0f;
        const float h = 40.0f;
        ports = {
            { 'T', ImVec2(node->x, node->y - h / 2.0f) }, // Top
            { 'B', ImVec2(node->x, node->y + h / 2.0f) }, // Bottom
            { 'L', ImVec2(node->x - w / 2.0f, node->y) }, // Left
            { 'R', ImVec2(node->x + w / 2.0f, node->y) }  // Right
        };
    }
    return ports;
}

PortPos GetClosestPort(std::shared_ptr<DesktopNode> nodeThis, std::shared_ptr<DesktopNode> nodeOther) {
    auto ports = GetPortPositions(nodeThis);
    PortPos bestPort = ports[0];
    float minDist = 1e9f;
    for (const auto& p : ports) {
        float dist = std::hypot(p.pos.x - nodeOther->x, p.pos.y - nodeOther->y);
        if (dist < minDist) {
            minDist = dist;
            bestPort = p;
        }
    }
    return bestPort;
}

std::vector<ImVec2> ComputeOrthogonalRoute(ImVec2 portA, char faceA, ImVec2 portB, char faceB) {
    std::vector<ImVec2> route;
    route.push_back(portA);
    
    // Normal vectors
    ImVec2 dirA(0, 0);
    if (faceA == 'T') dirA = ImVec2(0, -1);
    else if (faceA == 'B') dirA = ImVec2(0, 1);
    else if (faceA == 'L') dirA = ImVec2(-1, 0);
    else if (faceA == 'R') dirA = ImVec2(1, 0);
    
    ImVec2 dirB(0, 0);
    if (faceB == 'T') dirB = ImVec2(0, -1);
    else if (faceB == 'B') dirB = ImVec2(0, 1);
    else if (faceB == 'L') dirB = ImVec2(-1, 0);
    else if (faceB == 'R') dirB = ImVec2(1, 0);
    
    const float clearance = 15.0f;
    ImVec2 s1(portA.x + dirA.x * clearance, portA.y + dirA.y * clearance);
    ImVec2 s2(portB.x + dirB.x * clearance, portB.y + dirB.y * clearance);
    
    route.push_back(s1);
    
    // Check orientation cases
    bool isParallel = (dirA.x * dirB.x + dirA.y * dirB.y) != 0.0f; // Dot product != 0 => parallel or anti-parallel
    bool isOpposing = (dirA.x * dirB.x + dirA.y * dirB.y) < -0.9f; // Dot product = -1 => opposing
    
    if (isParallel) {
        if (dirA.x != 0.0f) { // Horizontal exits (Left/Right)
            if (isOpposing) {
                if ((dirA.x > 0.0f && s1.x <= s2.x) || (dirA.x < 0.0f && s1.x >= s2.x)) {
                    float midX = 0.5f * (s1.x + s2.x);
                    route.push_back(ImVec2(midX, s1.y));
                    route.push_back(ImVec2(midX, s2.y));
                } else {
                    float midY = 0.5f * (s1.y + s2.y);
                    if (std::abs(s1.y - s2.y) < 40.0f) {
                        midY = s1.y + (s1.y > s2.y ? 40.0f : -40.0f);
                    }
                    route.push_back(ImVec2(s1.x, midY));
                    route.push_back(ImVec2(s2.x, midY));
                }
            } else { // Same direction
                float xOut = (dirA.x > 0.0f) ? std::max(s1.x, s2.x) : std::min(s1.x, s2.x);
                route.push_back(ImVec2(xOut, s1.y));
                route.push_back(ImVec2(xOut, s2.y));
            }
        } else { // Vertical exits (Top/Bottom)
            if (isOpposing) {
                if ((dirA.y > 0.0f && s1.y <= s2.y) || (dirA.y < 0.0f && s1.y >= s2.y)) {
                    float midY = 0.5f * (s1.y + s2.y);
                    route.push_back(ImVec2(s1.x, midY));
                    route.push_back(ImVec2(s2.x, midY));
                } else {
                    float midX = 0.5f * (s1.x + s2.x);
                    if (std::abs(s1.x - s2.x) < 70.0f) {
                        midX = s1.x + (s1.x > s2.x ? 70.0f : -70.0f);
                    }
                    route.push_back(ImVec2(midX, s1.y));
                    route.push_back(ImVec2(midX, s2.y));
                }
            } else { // Same direction
                float yOut = (dirA.y > 0.0f) ? std::max(s1.y, s2.y) : std::min(s1.y, s2.y);
                route.push_back(ImVec2(s1.x, yOut));
                route.push_back(ImVec2(s2.x, yOut));
            }
        }
    } else { // Perpendicular exits
        if (dirA.x != 0.0f) {
            route.push_back(ImVec2(s2.x, s1.y));
        } else {
            route.push_back(ImVec2(s1.x, s2.y));
        }
    }
    
    route.push_back(s2);
    route.push_back(portB);
    
    std::vector<ImVec2> clean_route;
    for (const auto& pt : route) {
        if (clean_route.empty()) {
            clean_route.push_back(pt);
        } else {
            ImVec2 last = clean_route.back();
            if (std::abs(last.x - pt.x) < 0.1f && std::abs(last.y - pt.y) < 0.1f) {
                continue;
            }
            if (clean_route.size() >= 2) {
                ImVec2 prev = clean_route[clean_route.size() - 2];
                bool is_h = (std::abs(prev.y - last.y) < 0.1f && std::abs(last.y - pt.y) < 0.1f);
                bool is_v = (std::abs(prev.x - last.x) < 0.1f && std::abs(last.x - pt.x) < 0.1f);
                if (is_h || is_v) {
                    clean_route.pop_back();
                }
            }
            clean_route.push_back(pt);
        }
    }
    return clean_route;
}

// Re-initialize solver model from local vector list
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

// Reset history list
void ResetHistory() {
    g_time_history.clear();
    g_temp_history.clear();
    
    // Add initial points
    g_time_history.push_back(g_sim_time);
    for (const auto& n : g_nodes) {
        g_temp_history[n->id].push_back(n->temp);
    }
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

// Update simulation measurements
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

// --- PRESET SCENARIO LOADING (C++ DESKTOP VERSION) ---
void LoadPreset(const std::string& key) {
    g_active_preset = key;
    g_force_tab_generic = true;
    g_nodes.clear();
    g_links.clear();
    g_sliders.clear();
    g_selected_node = nullptr;
    g_selected_link = nullptr;
    g_sim_time = 0.0;
    g_is_running = false;
    g_undo_stack.clear();
    g_redo_stack.clear();
    g_canvas_zoom = 1.0f;
    g_canvas_scrolling = ImVec2(0.0f, 0.0f);
    
    if (key == "vehicle") {
        auto engine = std::make_shared<EngineBlockNode>();
        engine->id = 100;
        engine->x = 180.0f;
        engine->y = 180.0f;
        engine->jacketNode->id = 101;
        engine->internalCond->id = 102;
        engine->internalCond->node_a = 100;
        engine->internalCond->node_b = 101;
        engine->capacity = engine->params["block_capacity"];
        engine->q_gen = engine->params["heat_rejection"];
        engine->jacketNode->fluid_volume = engine->params["jacket_volume"];
        engine->internalCond->p1 = engine->params["block_jacket_cond"];
        g_nodes.push_back(engine);

        auto radiator = std::make_shared<RadiatorNode>();
        radiator->id = 200;
        radiator->x = 500.0f;
        radiator->y = 180.0f;
        radiator->coreNode->id = 201;
        radiator->internalConv->id = 202;
        radiator->internalConv->node_a = 200;
        radiator->internalConv->node_b = 201;
        radiator->fluid_volume = radiator->params["coolant_volume"];
        radiator->coreNode->capacity = radiator->params["core_capacity"];
        radiator->internalConv->p1 = radiator->params["coolant_hA"];
        g_nodes.push_back(radiator);

        auto ambient = std::make_shared<AmbientAirNode>();
        ambient->id = 300;
        ambient->x = 680.0f;
        ambient->y = 100.0f;
        ambient->temp = ambient->params["temp_c"];
        g_nodes.push_back(ambient);

        auto l_flow1 = std::make_shared<DesktopLink>();
        l_flow1->id = 104;
        l_flow1->node_a = 100;
        l_flow1->node_b = 200;
        l_flow1->type = 3;
        l_flow1->p1 = 400.0;
        l_flow1->p2 = 1.0;
        g_links.push_back(l_flow1);

        auto l_flow2 = std::make_shared<DesktopLink>();
        l_flow2->id = 105;
        l_flow2->node_a = 200;
        l_flow2->node_b = 100;
        l_flow2->type = 3;
        l_flow2->p1 = 400.0;
        l_flow2->p2 = 1.0;
        g_links.push_back(l_flow2);

        auto l_conv = std::make_shared<DesktopLink>();
        l_conv->id = 103;
        l_conv->node_a = 200;
        l_conv->node_b = 300;
        l_conv->type = 1;
        l_conv->p1 = 500.0;
        l_conv->p2 = 0.0;
        g_links.push_back(l_conv);

        g_sliders = {
            { "engine_power", "Engine Heat Output (kW)", 0.0f, 80.0f, 15.0f, 2.0f, "node_kw", 100, {}, "q_gen" },
            { "pump_speed", "Water Pump Rate (W/K)", 0.0f, 2000.0f, 400.0f, 100.0f, "links_all_flow", 0, {104, 105}, "p1" },
            { "radiator_fan", "Radiator Cooling Fan (W/K)", 50.0f, 3000.0f, 500.0f, 50.0f, "link", 103, {}, "p1" },
            { "ambient_temp", "Ambient Environment Temp (C)", 5.0f, 48.0f, 30.0f, 1.0f, "node", 300, {}, "temp" }
        };
    } 
    else if (key == "cpu") {
        struct NodeInit { int id; std::string name; float x, y; double temp, capacity, q_gen; bool is_fixed; };
        std::vector<NodeInit> nodes_init = {
            { 1, "CPU Die", 140.0f, 180.0f, 40.0, 150.0, 85.0, false },
            { 2, "TIM Thermal Paste", 300.0f, 180.0f, 32.0, 15.0, 0.0, false },
            { 3, "Copper Heat Sink", 460.0f, 180.0f, 28.0, 350.0, 0.0, false },
            { 4, "Chassis Air", 620.0f, 180.0f, 25.0, 1.0, 0.0, true }
        };
        for (const auto& ni : nodes_init) {
            auto n = std::make_shared<DesktopNode>();
            n->id = ni.id; n->name = ni.name; n->x = ni.x; n->y = ni.y;
            n->temp = ni.temp; n->capacity = ni.capacity; n->q_gen = ni.q_gen; n->is_fixed = ni.is_fixed;
            n->temp_init = ni.temp;
            g_nodes.push_back(n);
        }
        struct LinkInit { int id; int node_a; int node_b; int type; double p1; double p2; };
        std::vector<LinkInit> links_init = {
            { 101, 1, 2, 0, 80.0, 0.0 },
            { 102, 2, 3, 0, 50.0, 0.0 },
            { 103, 3, 4, 1, 4.0, 0.0 }
        };
        for (const auto& li : links_init) {
            auto l = std::make_shared<DesktopLink>();
            l->id = li.id; l->node_a = li.node_a; l->node_b = li.node_b; l->type = li.type; l->p1 = li.p1; l->p2 = li.p2;
            g_links.push_back(l);
        }
        g_sliders = {
            { "cpu_power", "CPU TDP Heat (W)", 0.0f, 200.0f, 85.0f, 5.0f, "node", 1, {}, "q_gen" },
            { "fan_speed", "Heat Sink Convection (W/K)", 0.1f, 20.0f, 4.0f, 0.5f, "link", 103, {}, "p1" }
        };
    } 
    else if (key == "battery") {
        struct NodeInit { int id; std::string name; float x, y; double temp, capacity, q_gen; bool is_fixed; };
        std::vector<NodeInit> nodes_init = {
            { 1, "Battery Cell 1", 180.0f, 100.0f, 25.0, 600.0, 20.0, false },
            { 2, "Battery Cell 2", 300.0f, 100.0f, 25.0, 600.0, 20.0, false },
            { 3, "Battery Cell 3", 420.0f, 100.0f, 25.0, 600.0, 20.0, false },
            { 4, "Battery Cell 4", 540.0f, 100.0f, 25.0, 600.0, 20.0, false },
            { 5, "Coolant Channel 1", 180.0f, 240.0f, 20.0, 100.0, 0.0, false },
            { 6, "Coolant Channel 2", 300.0f, 240.0f, 20.0, 100.0, 0.0, false },
            { 7, "Coolant Channel 3", 420.0f, 240.0f, 20.0, 100.0, 0.0, false },
            { 8, "Coolant Channel 4", 540.0f, 240.0f, 20.0, 100.0, 0.0, false },
            { 9, "Coolant Inlet", 60.0f, 240.0f, 20.0, 1.0, 0.0, true },
            { 10, "Coolant Outlet", 660.0f, 240.0f, 20.0, 1.0, 0.0, true }
        };
        for (const auto& ni : nodes_init) {
            auto n = std::make_shared<DesktopNode>();
            n->id = ni.id; n->name = ni.name; n->x = ni.x; n->y = ni.y;
            n->temp = ni.temp; n->capacity = ni.capacity; n->q_gen = ni.q_gen; n->is_fixed = ni.is_fixed;
            n->temp_init = ni.temp;
            if (ni.id >= 5 && ni.id <= 8) { n->domain = 1; n->fluid_medium = "Water"; n->fluid_volume = 0.5; }
            g_nodes.push_back(n);
        }
        struct LinkInit { int id; int node_a; int node_b; int type; double p1; double p2; };
        std::vector<LinkInit> links_init = {
            { 101, 1, 2, 0, 0.8, 0.0 },
            { 102, 2, 3, 0, 0.8, 0.0 },
            { 103, 3, 4, 0, 0.8, 0.0 },
            { 104, 1, 5, 1, 12.0, 0.0 },
            { 105, 2, 6, 1, 12.0, 0.0 },
            { 106, 3, 7, 1, 12.0, 0.0 },
            { 107, 4, 8, 1, 12.0, 0.0 },
            { 108, 9, 5, 3, 8.0, 1.0 },
            { 109, 5, 6, 3, 8.0, 1.0 },
            { 110, 6, 7, 3, 8.0, 1.0 },
            { 111, 7, 8, 3, 8.0, 1.0 },
            { 112, 8, 10, 3, 8.0, 1.0 }
        };
        for (const auto& li : links_init) {
            auto l = std::make_shared<DesktopLink>();
            l->id = li.id; l->node_a = li.node_a; l->node_b = li.node_b; l->type = li.type; l->p1 = li.p1; l->p2 = li.p2;
            g_links.push_back(l);
        }
        g_sliders = {
            { "battery_heat", "Cell Dissipation (W/cell)", 0.0f, 50.0f, 20.0f, 2.0f, "nodes_all_q", 0, {1,2,3,4}, "q_gen" },
            { "coolant_flow", "Fluid Heat Capacity Flow (W/K)", 0.5f, 25.0f, 8.0f, 0.5f, "links_all_flow", 0, {108,109,110,111,112}, "p1" },
            { "inlet_temp", "Coolant Feed Temp (C)", 5.0f, 35.0f, 20.0f, 1.0f, "node", 9, {}, "temp" }
        };
    } 
    else if (key == "window") {
        struct NodeInit { int id; std::string name; float x, y; double temp, capacity, q_gen; bool is_fixed; };
        std::vector<NodeInit> nodes_init = {
            { 1, "Indoor Room Air", 100.0f, 180.0f, 22.0, 1.0, 0.0, true },
            { 2, "Inner Pane Glass", 260.0f, 180.0f, 16.0, 500.0, 0.0, false },
            { 3, "Gas Gap (Argon)", 420.0f, 180.0f, 8.0, 20.0, 0.0, false },
            { 4, "Outer Pane Glass", 580.0f, 180.0f, 1.0, 500.0, 0.0, false },
            { 5, "Outdoor Atmosphere", 740.0f, 180.0f, -8.0, 1.0, 0.0, true }
        };
        for (const auto& ni : nodes_init) {
            auto n = std::make_shared<DesktopNode>();
            n->id = ni.id; n->name = ni.name; n->x = ni.x; n->y = ni.y;
            n->temp = ni.temp; n->capacity = ni.capacity; n->q_gen = ni.q_gen; n->is_fixed = ni.is_fixed;
            n->temp_init = ni.temp;
            g_nodes.push_back(n);
        }
        struct LinkInit { int id; int node_a; int node_b; int type; double p1; double p2; };
        std::vector<LinkInit> links_init = {
            { 101, 1, 2, 1, 8.0, 0.0 },
            { 102, 2, 3, 0, 3.5, 0.0 },
            { 103, 3, 4, 0, 3.5, 0.0 },
            { 104, 4, 5, 1, 30.0, 0.0 }
        };
        for (const auto& li : links_init) {
            auto l = std::make_shared<DesktopLink>();
            l->id = li.id; l->node_a = li.node_a; l->node_b = li.node_b; l->type = li.type; l->p1 = li.p1; l->p2 = li.p2;
            g_links.push_back(l);
        }
        g_sliders = {
            { "indoor_temp", "Indoor Thermostat (C)", 16.0f, 28.0f, 22.0f, 0.5f, "node", 1, {}, "temp" },
            { "outdoor_temp", "Outdoor Temperature (C)", -35.0f, 15.0f, -8.0f, 1.0f, "node", 5, {}, "temp" },
            { "wind_convection", "Wind Heat Transfer (W/K)", 5.0f, 100.0f, 30.0f, 2.0f, "link", 104, {}, "p1" }
        };
    }

    g_plot_active_nodes.clear();
    for (const auto& n : g_nodes) {
        g_plot_active_nodes[n->id] = true;
    }

    for (auto& n : g_nodes) {
        n->temp_init = n->temp;
    }

    ApplySlidersToSystem();
    SyncSystemWithSolver();
    ResetHistory();
    Log("Preset loaded: " + key, "success");
}

// Perform step on solver and update desktop coordinates temperatures
void StepSimulation() {
    // Dynamic updates: apply slider values to nodes and links
    ApplySlidersToSystem();

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

// Steady State Solver trigger
void SolveSteadyState() {
    // Sync slider variables first
    SyncSystemWithSolver();
    
    int iters = g_solver.solve_steady_state(1e-6, 2000);
    
    for (auto& n : g_nodes) {
        n->temp = kToC(g_solver.get_node_temperature(n->id));
    }
    
    UpdateHistory();
    Log("Steady State Solver converged in " + std::to_string(iters) + " sweeps.", "success");
}

// Export simulation time steps to CSV file
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

// --- DESKTOP RENDER LOOP ---
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
    
    // 4. Programmatic docking layout builder on first frame initialization
    static bool dockspace_initialized = false;
    if (!dockspace_initialized) {
        dockspace_initialized = true;
        
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
    
    ImGui::End(); // End Main Window

    // --- PANEL 2: COMPONENT LIBRARY & TREE EXPLORER (Left) ---
    ImGui::Begin("Object Explorer", nullptr);
    {
        // Unified Component Palette
        if (ImGui::CollapsingHeader("Component Palette", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(2, "palette_cols", false);
            
            // Physical components
            for (const auto& def : GetComponentLibrary()) {
                if (def.category == "ICE") {
                    ImGui::PushID(def.id.c_str());
                    if (ImGui::Button(def.name.c_str(), ImVec2(90.0f, 40.0f))) {
                        g_placing_component = true;
                        g_pending_comp_type = def.id;
                        setTool(0);
                    }
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("COMP_LIBRARY_ITEM", def.id.c_str(), def.id.size() + 1);
                        ImGui::Text("Drop %s", def.name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", def.description.c_str());
                    ImGui::PopID();
                    ImGui::NextColumn();
                }
            }

            // Generic elements
            // Thermal Mass
            ImGui::PushID("generic_mass");
            if (ImGui::Button("Thermal Mass\n(Node)", ImVec2(90.0f, 40.0f))) {
                g_placing_component = true;
                g_pending_comp_type = "generic_mass";
                setTool(0);
            }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("COMP_LIBRARY_ITEM", "generic_mass", strlen("generic_mass") + 1);
                ImGui::Text("Drop Thermal Mass");
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click or drag to place a generic solid Thermal Mass node.");
            ImGui::PopID();
            ImGui::NextColumn();

            // Fixed Boundary
            ImGui::PushID("generic_boundary");
            if (ImGui::Button("Fixed Bound\n(Node)", ImVec2(90.0f, 40.0f))) {
                g_placing_component = true;
                g_pending_comp_type = "generic_boundary";
                setTool(0);
            }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("COMP_LIBRARY_ITEM", "generic_boundary", strlen("generic_boundary") + 1);
                ImGui::Text("Drop Fixed Boundary");
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click or drag to place a generic Fixed Temperature boundary node.");
            ImGui::PopID();
            ImGui::NextColumn();

            // Conduction Link
            ImGui::PushID("generic_conduction");
            if (ImGui::Button("Conduction\n(Link)", ImVec2(90.0f, 40.0f))) {
                setTool(1);
                g_pending_link_type = 0;
                Log("Connect Link tool active: Conduction", "info");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click then connect two nodes to create a Conduction link.");
            ImGui::PopID();
            ImGui::NextColumn();

            // Convection Link
            ImGui::PushID("generic_convection");
            if (ImGui::Button("Convection\n(Link)", ImVec2(90.0f, 40.0f))) {
                setTool(1);
                g_pending_link_type = 1;
                Log("Connect Link tool active: Convection", "info");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click then connect two nodes to create a Convection link.");
            ImGui::PopID();
            ImGui::NextColumn();

            // Radiation Link
            ImGui::PushID("generic_radiation");
            if (ImGui::Button("Radiation\n(Link)", ImVec2(90.0f, 40.0f))) {
                setTool(1);
                g_pending_link_type = 2;
                Log("Connect Link tool active: Radiation", "info");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click then connect two nodes to create a Radiation link.");
            ImGui::PopID();
            ImGui::NextColumn();

            // Flow Loop Link
            ImGui::PushID("generic_flow");
            if (ImGui::Button("Flow Loop\n(Link)", ImVec2(90.0f, 40.0f))) {
                setTool(1);
                g_pending_link_type = 3;
                Log("Connect Link tool active: Coolant Flow", "info");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click then connect two nodes to create a Coolant Flow link.");
            ImGui::PopID();
            ImGui::NextColumn();

            ImGui::Columns(1);
        }

        ImGui::Separator();

        // 3. Model Directory Tree
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
    ImGui::End();

    // --- PANEL 3: SCHEMATIC CAD CANVAS (Center) ---
    ImGui::Begin("Schematic Diagram Canvas", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        // Canvas Toolbar
        if (ImGui::RadioButton("Select", g_current_tool == 0)) { setTool(0); g_placing_component=false; g_pending_comp_type=""; }
        ImGui::SameLine();
        if (true) {
            if (ImGui::RadioButton("Connect Link", g_current_tool == 1)) { setTool(1); }
            ImGui::SameLine();
        } else {
            if (ImGui::RadioButton("Connect Ports", g_current_tool == 3)) { g_current_tool=3; g_conn_from_inst=-1; g_conn_from_port=""; }
            ImGui::SameLine();
            if (g_placing_component) {
                ImGui::TextColored(ImVec4(0.2f,1.0f,0.4f,1.0f), "[PLACING: %s] — Click canvas to drop | ESC to cancel",
                    g_pending_comp_type.c_str());
            }
            ImGui::SameLine();
        }
        ImGui::TextDisabled(" | Zoom: %.0f%%  |  %s mode", g_canvas_zoom * 100.0f,
            "Simulation");
        
        ImGui::Separator();
        
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
        if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;
        
        // Draw grid boundaries
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(255, 255, 255, 255));
        draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(180, 180, 190, 255));
        
        ImGui::InvisibleButton("canvas_click_region", canvas_size);
        
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("COMP_LIBRARY_ITEM")) {
                const char* defId = (const char*)payload->Data;
                ImVec2 drop_mouse_pos = ImGui::GetMousePos();
                ImVec2 local_drop = ScreenToCanvas(drop_mouse_pos, canvas_pos);
                if (g_grid_snap) {
                    local_drop.x = std::round(local_drop.x / GRID_SIZE) * GRID_SIZE;
                    local_drop.y = std::round(local_drop.y / GRID_SIZE) * GRID_SIZE;
                }
                PlaceComponent(defId, local_drop.x, local_drop.y);
                g_placing_component = false;
                g_pending_comp_type = "";
                Log("Dropped component " + std::string(defId) + " onto canvas.", "success");
            }
            ImGui::EndDragDropTarget();
        }
        
        // Canvas interactions
        bool is_hovered = ImGui::IsItemHovered();
        ImVec2 mouse_pos = ImGui::GetMousePos();
        
        // Handle Zooming (Mouse Wheel)
        ImGuiIO& io = ImGui::GetIO();
        if (is_hovered && io.MouseWheel != 0.0f) {
            float old_zoom = g_canvas_zoom;
            g_canvas_zoom += io.MouseWheel * 0.075f;
            if (g_canvas_zoom < 0.25f) g_canvas_zoom = 0.25f;
            if (g_canvas_zoom > 3.0f) g_canvas_zoom = 3.0f;
            
            // Zoom centering on mouse pointer
            g_canvas_scrolling.x += (mouse_pos.x - canvas_pos.x) * (1.0f / g_canvas_zoom - 1.0f / old_zoom);
            g_canvas_scrolling.y += (mouse_pos.y - canvas_pos.y) * (1.0f / g_canvas_zoom - 1.0f / old_zoom);
        }
        
        // Handle Panning (Right/Middle drag)
        if (is_hovered && (ImGui::IsMouseDragging(1, 0.0f) || ImGui::IsMouseDragging(2, 0.0f))) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGui::IsMouseDragging(1, 0.0f) ? 1 : 2, 0.0f);
            g_canvas_scrolling.x += delta.x / g_canvas_zoom;
            g_canvas_scrolling.y += delta.y / g_canvas_zoom;
            ImGui::ResetMouseDragDelta(ImGui::IsMouseDragging(1, 0.0f) ? 1 : 2);
        }
        
        // Local mouse coordinates projection
        ImVec2 local_mouse = ScreenToCanvas(mouse_pos, canvas_pos);
        ImVec2 rel_mouse = local_mouse; // for backward compatibility with naming
        
        // Draw dotted grid relative to scrolling and zoom
        ImVec2 local_min = ScreenToCanvas(canvas_pos, canvas_pos);
        ImVec2 local_max = ScreenToCanvas(ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), canvas_pos);
        
        float start_x = std::floor(local_min.x / GRID_SIZE) * GRID_SIZE;
        float start_y = std::floor(local_min.y / GRID_SIZE) * GRID_SIZE;
        
        for (float lx = start_x; lx <= local_max.x; lx += GRID_SIZE) {
            for (float ly = start_y; ly <= local_max.y; ly += GRID_SIZE) {
                ImVec2 sp = CanvasToScreen(ImVec2(lx, ly), canvas_pos);
                draw_list->AddCircleFilled(sp, 1.0f * g_canvas_zoom, IM_COL32(200, 200, 210, 255));
            }
        }
        
        // --- DRAW CONNECTIONS ---
        for (const auto& l : g_links) {
            auto itA = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == l->node_a; });
            auto itB = std::find_if(g_nodes.begin(), g_nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == l->node_b; });
            if (itA == g_nodes.end() || itB == g_nodes.end()) continue;
            
            PortPos portA = GetClosestPort(*itA, *itB);
            PortPos portB = GetClosestPort(*itB, *itA);
            
            std::vector<ImVec2> route = ComputeOrthogonalRoute(portA.pos, portA.name, portB.pos, portB.name);
            std::vector<ImVec2> screenRoute;
            for (const auto& pt : route) {
                screenRoute.push_back(CanvasToScreen(pt, canvas_pos));
            }
            
            bool is_link_selected = (g_selected_link != nullptr && g_selected_link->id == l->id);
            ImU32 linkColor = IM_COL32(71, 85, 105, 255); // Gray Conduction
            
            if (l->type == 1)      linkColor = IM_COL32(21, 128, 61, 255);   // Green Convection
            else if (l->type == 2) linkColor = IM_COL32(234, 88, 12, 255);   // Orange Radiation
            else if (l->type == 3) linkColor = IM_COL32(2, 132, 199, 255);   // Blue Coolant Flow
            else if (l->type == 4) linkColor = IM_COL32(124, 58, 237, 255);  // Violet Fan / Pump Link
            
            // Draw link line segments
            for (size_t i = 0; i < screenRoute.size() - 1; ++i) {
                if (is_link_selected) {
                    draw_list->AddLine(screenRoute[i], screenRoute[i+1], IM_COL32(2, 92, 162, 50), 6.0f * g_canvas_zoom);
                }
                draw_list->AddLine(screenRoute[i], screenRoute[i+1], linkColor, (is_link_selected ? 3.0f : 1.5f) * g_canvas_zoom);
            }
            
            // Draw animated flow chevrons
            if ((l->type == 3 || l->type == 4) && g_is_running) {
                for (size_t i = 0; i < screenRoute.size() - 1; ++i) {
                    size_t start_idx = i;
                    size_t end_idx = i + 1;
                    double dir = l->p2;
                    if (l->type == 4) {
                        dir = l->p2 >= 0.0 ? 1.0 : -1.0;
                    }
                    if (dir < 0.0) {
                        start_idx = i + 1;
                        end_idx = i;
                    }
                    float dx = screenRoute[end_idx].x - screenRoute[start_idx].x;
                    float dy = screenRoute[end_idx].y - screenRoute[start_idx].y;
                    float length = std::hypot(dx, dy);
                    if (length > 0.0f) {
                        dx /= length;
                        dy /= length;
                        
                        float spacing = 20.0f * g_canvas_zoom;
                        float time_offset = (float)fmod(ImGui::GetTime() * 30.0f * g_canvas_zoom, spacing);
                        if (time_offset < 0.0f) time_offset += spacing;
                        
                        for (float d = time_offset; d < length; d += spacing) {
                            ImVec2 arrowPos = ImVec2(screenRoute[start_idx].x + dx * d, screenRoute[start_idx].y + dy * d);
                            draw_list->AddCircleFilled(arrowPos, 2.0f * g_canvas_zoom, linkColor);
                        }
                    }
                }
            }
            
            // Parameter Box inside link
            if (g_canvas_zoom >= 0.6f && screenRoute.size() >= 2) {
                // Find longest segment to center the parameter box
                size_t longest_idx = 0;
                float max_len = -1.0f;
                for (size_t i = 0; i < route.size() - 1; ++i) {
                    float len = std::hypot(route[i+1].x - route[i].x, route[i+1].y - route[i].y);
                    if (len > max_len) {
                        max_len = len;
                        longest_idx = i;
                    }
                }
                
                ImVec2 mid = ImVec2((screenRoute[longest_idx].x + screenRoute[longest_idx+1].x) / 2.0f, (screenRoute[longest_idx].y + screenRoute[longest_idx+1].y) / 2.0f);
                draw_list->AddRectFilled(ImVec2(mid.x - 14, mid.y - 7), ImVec2(mid.x + 14, mid.y + 7), IM_COL32(255, 255, 255, 255), 2.0f);
                draw_list->AddRect(ImVec2(mid.x - 14, mid.y - 7), ImVec2(mid.x + 14, mid.y + 7), linkColor, 2.0f);
                
                char paramText[16];
                if (l->p1 >= 1000.0) sprintf(paramText, "%.1fk", l->p1 / 1000.0);
                else sprintf(paramText, "%.0f", l->p1);
                
                draw_list->AddText(ImVec2(mid.x - 10, mid.y - 6), linkColor, paramText);
            }
        }
        
        // --- DRAW NODE BLOCKS ---
        for (auto& n : g_nodes) {
            float w, h;
            n->GetBounds(w, h);
            ImVec2 screenPos = CanvasToScreen(ImVec2(n->x, n->y), canvas_pos);
            ImVec2 topLeft = ImVec2(screenPos.x - (w * g_canvas_zoom) / 2.0f, screenPos.y - (h * g_canvas_zoom) / 2.0f);
            ImVec2 botRight = ImVec2(screenPos.x + (w * g_canvas_zoom) / 2.0f, screenPos.y + (h * g_canvas_zoom) / 2.0f);
            
            bool is_selected = (g_selected_node != nullptr && g_selected_node->id == n->id);
            
            // Draw polymorphic symbol
            n->DrawSymbol(draw_list, screenPos, g_canvas_zoom, is_selected, g_is_running, {});
            
            // Double line for generic fixed boundary (only for generic nodes, i.e., those without ports)
            if (n->is_fixed && n->GetPorts().empty()) {
                draw_list->AddRect(ImVec2(topLeft.x + 2 * g_canvas_zoom, topLeft.y + 2 * g_canvas_zoom), ImVec2(botRight.x - 2 * g_canvas_zoom, botRight.y - 2 * g_canvas_zoom), IM_COL32(180, 83, 9, 255), 1.0f, 3.0f * g_canvas_zoom);
            }
            
            // Draw Port terminals
            auto ports = GetPortPositions(n);
            for (const auto& p : ports) {
                ImVec2 sp = CanvasToScreen(p.pos, canvas_pos);
                draw_list->AddRectFilled(ImVec2(sp.x - 3 * g_canvas_zoom, sp.y - 3 * g_canvas_zoom), ImVec2(sp.x + 3 * g_canvas_zoom, sp.y + 3 * g_canvas_zoom), IM_COL32(2, 92, 162, 255));
                draw_list->AddRect(ImVec2(sp.x - 3 * g_canvas_zoom, sp.y - 3 * g_canvas_zoom), ImVec2(sp.x + 3 * g_canvas_zoom, sp.y + 3 * g_canvas_zoom), IM_COL32(255, 255, 255, 255), 1.0f);
            }
            
            // Draw text labels if zoom is large enough to read, otherwise render hover tooltips
            if (g_canvas_zoom >= 0.6f) {
                // Temp label text
                char tempText[32];
                sprintf(tempText, "%.1f C", n->temp);
                draw_list->AddText(ImVec2(screenPos.x - 18, screenPos.y - 8), IM_COL32(17, 24, 39, 255), tempText);
                
                // Name label text (bottom) - only for generic nodes
                if (n->GetPorts().empty()) {
                    draw_list->AddText(ImVec2(topLeft.x + 2, screenPos.y + 6), IM_COL32(55, 65, 81, 255), n->name.substr(0, 11).c_str());
                }
                
                // Heat source generation tag
                if (n->q_gen > 0.0) {
                    char qText[32];
                    if (n->q_gen >= 1000.0) sprintf(qText, "Q:%.1fkW", n->q_gen / 1000.0);
                    else sprintf(qText, "Q:%.0fW", n->q_gen);
                    draw_list->AddText(ImVec2(topLeft.x, topLeft.y - 12), IM_COL32(185, 28, 28, 255), qText);
                }
            } else if (is_hovered) {
                // Display node info tooltip when zoomed out
                if (local_mouse.x >= n->x - w / 2.0f && local_mouse.x <= n->x + w / 2.0f &&
                    local_mouse.y >= n->y - h / 2.0f && local_mouse.y <= n->y + h / 2.0f) {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.02f, 0.36f, 0.63f, 1.0f), "Node: %s", n->name.c_str());
                    ImGui::Separator();
                    ImGui::Text("Temperature: %.2f C", n->temp);
                    ImGui::Text("Heat Capacity: %.1f J/K", n->capacity);
                    ImGui::Text("Heat Source: %.1f W", n->q_gen);
                    ImGui::Text("Fixed Bound: %s", n->is_fixed ? "True" : "False");
                    ImGui::EndTooltip();
                }
            }
        }
        
        // Draw temporary connection line when creating link
        if (g_linking_start_node && g_current_tool == 1) {
            auto tempTargetNode = std::make_shared<DesktopNode>(); tempTargetNode->x = local_mouse.x; tempTargetNode->y = local_mouse.y;
            PortPos portA = GetClosestPort(g_linking_start_node, tempTargetNode);
            
            std::vector<ImVec2> previewRoute;
            previewRoute.push_back(portA.pos);
            
            if (portA.name == 'R' || portA.name == 'L') {
                float midX = 0.5f * (portA.pos.x + local_mouse.x);
                previewRoute.push_back(ImVec2(midX, portA.pos.y));
                previewRoute.push_back(ImVec2(midX, local_mouse.y));
            } else {
                float midY = 0.5f * (portA.pos.y + local_mouse.y);
                previewRoute.push_back(ImVec2(portA.pos.x, midY));
                previewRoute.push_back(ImVec2(local_mouse.x, midY));
            }
            previewRoute.push_back(local_mouse);
            
            for (size_t i = 0; i < previewRoute.size() - 1; ++i) {
                ImVec2 pStart = CanvasToScreen(previewRoute[i], canvas_pos);
                ImVec2 pEnd = CanvasToScreen(previewRoute[i+1], canvas_pos);
                draw_list->AddLine(pStart, pEnd, IM_COL32(2, 92, 162, 180), 1.5f * g_canvas_zoom);
            }
        }

        // --- CANVAS INTERACTIONS ---
        if (is_hovered) {
            if (ImGui::IsMouseClicked(0)) {
                auto hit_node = getNodeAt(local_mouse);
                if (g_placing_component) {
                    float targetX = local_mouse.x;
                    float targetY = local_mouse.y;
                    if (g_grid_snap) {
                        targetX = std::round(targetX / GRID_SIZE) * GRID_SIZE;
                        targetY = std::round(targetY / GRID_SIZE) * GRID_SIZE;
                    }
                    PlaceComponent(g_pending_comp_type, targetX, targetY);
                    g_placing_component = false;
                    g_pending_comp_type = "";
                } else if (g_current_tool == 0) { // Select tool
                    if (hit_node) {
                        g_selected_node = hit_node;
                        g_selected_link = nullptr;
                        isDragging = true;
                        dragNode = hit_node;
                        dragOffset.x = local_mouse.x - hit_node->x;
                        dragOffset.y = local_mouse.y - hit_node->y;
                        g_drag_backup.nodes = g_nodes;
                        g_drag_backup.links = g_links;
                        g_drag_backup_valid = true;
                    } else {
                        auto hit_link = getLinkAt(local_mouse);
                        if (hit_link) {
                            g_selected_link = hit_link;
                            g_selected_node = nullptr;
                        } else {
                            g_selected_node = nullptr;
                            g_selected_link = nullptr;
                        }
                    }
                } else if (g_current_tool == 1) { // Connect link tool
                    if (hit_node) {
                        g_linking_start_node = hit_node;
                    }
                }
            }
        }

        // Mouse Dragging
            if (isDragging && ImGui::IsMouseDragging(0)) {
                float targetX = local_mouse.x - dragOffset.x;
                float targetY = local_mouse.y - dragOffset.y;
                if (g_grid_snap) {
                    targetX = std::round(targetX / GRID_SIZE) * GRID_SIZE;
                    targetY = std::round(targetY / GRID_SIZE) * GRID_SIZE;
                }
                if (dragNode) {
                    dragNode->x = targetX;
                    dragNode->y = targetY;
                }
            }

            // Mouse Released
            if (ImGui::IsMouseReleased(0)) {
                if (isDragging) {
                    isDragging = false;
                    if (dragNode && g_drag_backup_valid) {
                        auto it = std::find_if(g_drag_backup.nodes.begin(), g_drag_backup.nodes.end(), [&](const std::shared_ptr<DesktopNode>& n) { return n->id == dragNode->id; });
                        if (it != g_drag_backup.nodes.end()) {
                            if ((*it)->x != dragNode->x || (*it)->y != dragNode->y) {
                                g_undo_stack.push_back(g_drag_backup);
                                if (g_undo_stack.size() > 50) g_undo_stack.erase(g_undo_stack.begin());
                                g_redo_stack.clear();
                                Log("Moved node component [" + dragNode->name + "]", "info");
                            }
                        }
                    }
                    dragNode = nullptr;
                    g_drag_backup_valid = false;
                }
                if (g_linking_start_node && g_current_tool == 1) {
                    auto hit_end = getNodeAt(local_mouse);
                    if (hit_end && hit_end->id != g_linking_start_node->id) {
                        g_show_link_modal = true;
                        g_modal_node_a_id = g_linking_start_node->id;
                        g_modal_node_b_id = hit_end->id;
                        g_modal_link_type = g_pending_link_type;
                        g_modal_link_p1 = 10.0;
                        g_modal_link_p2 = 1.0;
                    }
                    g_linking_start_node = nullptr;
                }
            }
        }
    ImGui::End();

    // --- PANEL 4: TABBED SIDEBAR (Right) ---
    ImGui::Begin("Inspector Panel", nullptr);
    {
        if (ImGui::BeginTabBar("InspectorTabBar")) {

            // Tab 1: Properties Spreadsheet (Attribute Sheet)
            if (ImGui::BeginTabItem("Attribute Sheet")) {
                ImGui::BeginChild("SS_ChildRegion");

                // ── LEGACY NODE/LINK ATTRIBUTE SHEET ───────────────────────────
                {
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
                                ImGui::TableNextColumn(); ImGui::Text("%");
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

                        // Draw custom physical component parameters
                        if (!g_selected_node->params.empty()) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextDisabled("Physical Params");
                            ImGui::TableNextColumn();
                            ImGui::TextDisabled("---");
                            ImGui::TableNextColumn();
                            ImGui::TextDisabled("---");
                            ImGui::TableNextColumn();
                            ImGui::TextDisabled("---");

                            for (auto& pair : g_selected_node->params) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("%s", pair.first.c_str());
                                ImGui::TableNextColumn();
                                double val = pair.second;
                                ImGui::PushItemWidth(-FLT_MIN);
                                char label[64];
                                sprintf(label, "##param_%s", pair.first.c_str());
                                if (ImGui::InputDouble(label, &val, 1.0, 10.0, "%.2f")) {
                                    pair.second = val;
                                    if (pair.first == "heat_rejection") {
                                        g_selected_node->q_gen = val;
                                    } else if (pair.first == "block_capacity") {
                                        g_selected_node->capacity = val;
                                    } else if (pair.first == "block_jacket_cond") {
                                        auto block = std::dynamic_pointer_cast<EngineBlockNode>(g_selected_node);
                                        if (block && block->internalCond) {
                                            block->internalCond->p1 = val;
                                        }
                                    } else if (pair.first == "jacket_volume") {
                                        auto block = std::dynamic_pointer_cast<EngineBlockNode>(g_selected_node);
                                        if (block && block->jacketNode) {
                                            block->jacketNode->fluid_volume = val;
                                        }
                                    } else if (pair.first == "coolant_hA") {
                                        auto rad = std::dynamic_pointer_cast<RadiatorNode>(g_selected_node);
                                        if (rad && rad->internalConv) {
                                            rad->internalConv->p1 = val;
                                        }
                                    } else if (pair.first == "coolant_volume") {
                                        g_selected_node->fluid_volume = val;
                                    } else if (pair.first == "core_capacity") {
                                        auto rad = std::dynamic_pointer_cast<RadiatorNode>(g_selected_node);
                                        if (rad && rad->coreNode) {
                                            rad->coreNode->capacity = val;
                                        }
                                    } else if (pair.first == "temp_c") {
                                        g_selected_node->temp = val;
                                        g_selected_node->temp_init = val;
                                    }
                                    SyncSystemWithSolver();
                                }
                                if (ImGui::IsItemActivated()) { PushUndoState(); }
                                ImGui::PopItemWidth();
                                ImGui::TableNextColumn();
                                ImGui::Text("-");
                                ImGui::TableNextColumn();
                                ImGui::Text("Component parameter");
                            }
                        }
                        
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
                        ImGui::TableNextColumn(); ImGui::Text("%s", p1Label.c_str());
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
                        ImGui::TableNextColumn(); ImGui::Text("%s", p1Unit.c_str());
                        ImGui::TableNextColumn(); ImGui::Text("%s", (p1Desc + " (a0 term)").c_str());
                        
                        // Parameter 1 Linear term
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%s", g_selected_link->type == 4 ? "Resist Coeff K" : (p1Label + " Coeff a1").c_str());
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
                        ImGui::TableNextColumn(); ImGui::Text("%s", g_a1Unit.c_str());
                        ImGui::TableNextColumn(); ImGui::Text("%s", g_selected_link->type == 4 ? "System quadratic flow resistance" : "Linear temp coefficient a1");
                        
                        // Parameter 1 Quadratic term
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::Text("%s", g_selected_link->type == 4 ? "Resist Coeff R" : (p1Label + " Coeff a2").c_str());
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
                        ImGui::TableNextColumn(); ImGui::Text("%s", g_a2Unit.c_str());
                        ImGui::TableNextColumn(); ImGui::Text("%s", g_selected_link->type == 4 ? "System linear flow resistance" : "Quadratic temp coefficient a2");
                        
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
            int nextId = g_links.empty() ? 101 : (*std::max_element(g_links.begin(), g_links.end(), [](const std::shared_ptr<DesktopLink>& a, const std::shared_ptr<DesktopLink>& b){ return a->id < b->id; }))->id + 1;
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
            g_selected_link = newLink;
            g_selected_node = nullptr;
            
            Log("Created connection link N:" + std::to_string(newLink->node_a) + " -> N:" + std::to_string(newLink->node_b), "info");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
} // End RenderUI()

void SaveModel() {
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"GT-Thermal Model (*.gtm)\0*.gtm\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrDefExt = L"gtm";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn) == TRUE) {
        try {
            nlohmann::json j;
            
            // Serialize nodes
            nlohmann::json j_nodes = nlohmann::json::array();
            for (const auto& n : g_nodes) {
                nlohmann::json jn;
                jn["id"] = n->id;
                jn["name"] = n->name;
                jn["x"] = n->x;
                jn["y"] = n->y;
                jn["temp"] = n->temp;
                jn["capacity"] = n->capacity;
                jn["q_gen"] = n->q_gen;
                jn["is_fixed"] = n->is_fixed;
                jn["temp_init"] = n->temp_init;
                jn["c_a1"] = n->c_a1;
                jn["c_a2"] = n->c_a2;
                jn["material"] = n->material;
                jn["mass"] = n->mass;
                jn["domain"] = n->domain;
                jn["fluid_medium"] = n->fluid_medium;
                jn["fluid_volume"] = n->fluid_volume;
                jn["fluid_mix_ratio"] = n->fluid_mix_ratio;
                jn["fluid_rho_a0"] = n->fluid_rho_a0;
                jn["fluid_rho_a1"] = n->fluid_rho_a1;
                jn["fluid_rho_a2"] = n->fluid_rho_a2;
                jn["fluid_cp_a0"] = n->fluid_cp_a0;
                jn["fluid_cp_a1"] = n->fluid_cp_a1;
                jn["fluid_cp_a2"] = n->fluid_cp_a2;
                if (!n->params.empty()) {
                    nlohmann::json jp = nlohmann::json::object();
                    for (const auto& pair : n->params) {
                        jp[pair.first] = pair.second;
                    }
                    jn["params"] = jp;
                }
                j_nodes.push_back(jn);
            }
            j["nodes"] = j_nodes;
            
            // Serialize links
            nlohmann::json j_links = nlohmann::json::array();
            for (const auto& l : g_links) {
                nlohmann::json jl;
                jl["id"] = l->id;
                jl["node_a"] = l->node_a;
                jl["node_b"] = l->node_b;
                jl["type"] = l->type;
                jl["p1"] = l->p1;
                jl["p2"] = l->p2;
                jl["g_a1"] = l->g_a1;
                jl["g_a2"] = l->g_a2;
                jl["fan_area"] = l->fan_area;
                j_links.push_back(jl);
            }
            j["links"] = j_links;
            
            // Serialize sliders
            nlohmann::json j_sliders = nlohmann::json::array();
            for (const auto& s : g_sliders) {
                nlohmann::json js;
                js["id"] = s.id;
                js["label"] = s.label;
                js["min_val"] = s.min_val;
                js["max_val"] = s.max_val;
                js["value"] = s.value;
                js["step"] = s.step;
                js["target_type"] = s.target_type;
                js["target_id"] = s.target_id;
                
                nlohmann::json j_target_ids = nlohmann::json::array();
                for (int tid : s.target_ids) {
                    j_target_ids.push_back(tid);
                }
                js["target_ids"] = j_target_ids;
                
                js["field"] = s.field;
                j_sliders.push_back(js);
            }
            j["sliders"] = j_sliders;
            
            // Write to file
            std::ofstream out(ofn.lpstrFile);
            if (out.is_open()) {
                out << j.dump(4);
                out.close();
                
                std::wstring ws(ofn.lpstrFile);
                std::string path_str(ws.begin(), ws.end());
                Log("Model saved to: " + path_str, "success");
            } else {
                Log("Failed to write model to file.", "error");
            }
        } catch (const std::exception& e) {
            Log("Error saving model: " + std::string(e.what()), "error");
        }
    }
}

void LoadModel() {
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"GT-Thermal Model (*.gtm)\0*.gtm\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn) == TRUE) {
        try {
            std::ifstream in(ofn.lpstrFile);
            if (!in.is_open()) {
                Log("Failed to open file.", "error");
                return;
            }
            
            nlohmann::json j;
            in >> j;
            in.close();
            
            PushUndoState();
            
            g_active_preset = "custom";
            g_nodes.clear();
            g_links.clear();
            g_sliders.clear();
            g_selected_node = nullptr;
            g_selected_link = nullptr;
            g_sim_time = 0.0;
            g_is_running = false;
            g_undo_stack.clear();
            g_redo_stack.clear();
            g_canvas_zoom = 1.0f;
            g_canvas_scrolling = ImVec2(0.0f, 0.0f);
            
            // Deserialize nodes
            if (j.contains("nodes") && j["nodes"].is_array()) {
                for (const auto& jn : j["nodes"]) {
                    std::string nodeName = jn["name"].get<std::string>();
                    std::shared_ptr<DesktopNode> n;
                    if (nodeName == "Engine Block") {
                        auto engine = std::make_shared<EngineBlockNode>();
                        engine->jacketNode->id = jn["id"] + 1;
                        engine->internalCond->id = jn["id"] + 2;
                        engine->internalCond->node_a = jn["id"];
                        engine->internalCond->node_b = engine->jacketNode->id;
                        n = engine;
                    } else if (nodeName == "Radiator") {
                        auto radiator = std::make_shared<RadiatorNode>();
                        radiator->coreNode->id = jn["id"] + 1;
                        radiator->internalConv->id = jn["id"] + 2;
                        radiator->internalConv->node_a = jn["id"];
                        radiator->internalConv->node_b = radiator->coreNode->id;
                        n = radiator;
                    } else if (nodeName == "Ambient Air") {
                        n = std::make_shared<AmbientAirNode>();
                    } else {
                        n = std::make_shared<DesktopNode>();
                    }
                    n->id = jn["id"];
                    n->name = nodeName;
                    n->x = jn["x"];
                    n->y = jn["y"];
                    n->temp = jn["temp"];
                    n->capacity = jn["capacity"];
                    n->q_gen = jn["q_gen"];
                    n->is_fixed = jn["is_fixed"];
                    
                    if (jn.contains("temp_init")) {
                        n->temp_init = jn["temp_init"];
                    } else {
                        n->temp_init = n->temp;
                    }
                    n->c_a1 = jn.contains("c_a1") ? jn["c_a1"].get<double>() : 0.0;
                    n->c_a2 = jn.contains("c_a2") ? jn["c_a2"].get<double>() : 0.0;
                    n->material = jn.contains("material") ? jn["material"].get<std::string>() : "Custom";
                    n->mass = jn.contains("mass") ? jn["mass"].get<double>() : 1.0;
                    n->domain = jn.contains("domain") ? jn["domain"].get<int>() : 0;
                    n->fluid_medium = jn.contains("fluid_medium") ? jn["fluid_medium"].get<std::string>() : "Water";
                    n->fluid_volume = jn.contains("fluid_volume") ? jn["fluid_volume"].get<double>() : 1.0;
                    n->fluid_mix_ratio = jn.contains("fluid_mix_ratio") ? jn["fluid_mix_ratio"].get<double>() : 0.5;
                    n->fluid_rho_a0 = jn.contains("fluid_rho_a0") ? jn["fluid_rho_a0"].get<double>() : 1000.0;
                    n->fluid_rho_a1 = jn.contains("fluid_rho_a1") ? jn["fluid_rho_a1"].get<double>() : 0.0;
                    n->fluid_rho_a2 = jn.contains("fluid_rho_a2") ? jn["fluid_rho_a2"].get<double>() : 0.0;
                    n->fluid_cp_a0 = jn.contains("fluid_cp_a0") ? jn["fluid_cp_a0"].get<double>() : 4184.0;
                    n->fluid_cp_a1 = jn.contains("fluid_cp_a1") ? jn["fluid_cp_a1"].get<double>() : 0.0;
                    n->fluid_cp_a2 = jn.contains("fluid_cp_a2") ? jn["fluid_cp_a2"].get<double>() : 0.0;

                    if (jn.contains("params") && jn["params"].is_object()) {
                        for (auto& item : jn["params"].items()) {
                            n->params[item.key()] = item.value().get<double>();
                        }
                        // Re-sync params to sub-node properties
                        if (nodeName == "Engine Block") {
                            auto engine = std::static_pointer_cast<EngineBlockNode>(n);
                            engine->capacity = engine->params["block_capacity"];
                            engine->q_gen = engine->params["heat_rejection"];
                            engine->jacketNode->fluid_volume = engine->params["jacket_volume"];
                            engine->internalCond->p1 = engine->params["block_jacket_cond"];
                        } else if (nodeName == "Radiator") {
                            auto radiator = std::static_pointer_cast<RadiatorNode>(n);
                            radiator->fluid_volume = radiator->params["coolant_volume"];
                            radiator->coreNode->capacity = radiator->params["core_capacity"];
                            radiator->internalConv->p1 = radiator->params["coolant_hA"];
                        } else if (nodeName == "Ambient Air") {
                            n->temp = n->params["temp_c"];
                            n->temp_init = n->params["temp_c"];
                        }
                    }
                    g_nodes.push_back(n);
                }
            }
            
            // Deserialize links
            if (j.contains("links") && j["links"].is_array()) {
                for (const auto& jl : j["links"]) {
                    auto l = std::make_shared<DesktopLink>();
                    l->id = jl["id"];
                    l->node_a = jl["node_a"];
                    l->node_b = jl["node_b"];
                    l->type = jl["type"];
                    l->p1 = jl["p1"];
                    l->p2 = jl["p2"];
                    l->g_a1 = jl.contains("g_a1") ? jl["g_a1"].get<double>() : 0.0;
                    l->g_a2 = jl.contains("g_a2") ? jl["g_a2"].get<double>() : 0.0;
                    l->fan_area = jl.contains("fan_area") ? jl["fan_area"].get<double>() : 0.005;
                    g_links.push_back(l);
                }
            }
            
            // Deserialize sliders
            if (j.contains("sliders") && j["sliders"].is_array()) {
                for (const auto& js : j["sliders"]) {
                    SliderConfig s;
                    s.id = js["id"].get<std::string>();
                    s.label = js["label"].get<std::string>();
                    s.min_val = js["min_val"];
                    s.max_val = js["max_val"];
                    s.value = js["value"];
                    s.step = js["step"];
                    s.target_type = js["target_type"].get<std::string>();
                    s.target_id = js["target_id"];
                    
                    s.target_ids.clear();
                    if (js.contains("target_ids") && js["target_ids"].is_array()) {
                        for (int tid : js["target_ids"]) {
                            s.target_ids.push_back(tid);
                        }
                    }
                    
                    s.field = js["field"].get<std::string>();
                    g_sliders.push_back(s);
                }
            }
            
            g_plot_active_nodes.clear();
            for (const auto& n : g_nodes) {
                g_plot_active_nodes[n->id] = true;
            }
            
            ApplySlidersToSystem();
            SyncSystemWithSolver();
            ResetHistory();
            
            std::wstring ws(ofn.lpstrFile);
            std::string path_str(ws.begin(), ws.end());
            Log("Model loaded successfully: " + path_str, "success");
        } catch (const std::exception& e) {
            Log("Error loading model: " + std::string(e.what()), "error");
        }
    }
}

// Clear implementation
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

// Reset implementation
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

// --- WIN32 ENTRY POINT & D3D DEVICE CODE ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Create Application Window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, L"GT_Thermal1D_WindowClass", nullptr };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"GT-Thermal 1D System Simulator", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show window
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);

    // Initialize ImGui and ImPlot
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    
    // Theme setup: Crisp, Professional Light Grey/White theme
    ImGui::StyleColorsLight();
    
    // Tweak Style colors to look like a desktop CAD
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 3.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 3.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    
    style.Colors[ImGuiCol_WindowBg]             = ImVec4(0.97f, 0.97f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_ChildBg]              = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    style.Colors[ImGuiCol_Border]               = ImVec4(0.69f, 0.71f, 0.76f, 1.0f);
    style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]              = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.90f, 0.93f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive]        = ImVec4(0.85f, 0.89f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_TitleBg]              = ImVec4(0.86f, 0.88f, 0.92f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive]        = ImVec4(0.80f, 0.83f, 0.88f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.86f, 0.88f, 0.92f, 1.0f);
    style.Colors[ImGuiCol_Header]               = ImVec4(0.90f, 0.93f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered]        = ImVec4(0.80f, 0.85f, 0.92f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive]         = ImVec4(0.70f, 0.80f, 0.90f, 1.0f);
    style.Colors[ImGuiCol_Tab]                  = ImVec4(0.86f, 0.88f, 0.92f, 1.0f);
    style.Colors[ImGuiCol_TabHovered]           = ImVec4(0.97f, 0.97f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_TabActive]            = ImVec4(0.97f, 0.97f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_Button]               = ImVec4(0.95f, 0.96f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.90f, 0.92f, 0.95f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive]         = ImVec4(0.85f, 0.88f, 0.92f, 1.0f);

    // Setup Platform/Renderer Backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load initial preset
    LoadPreset("vehicle");

    // Main Win32 Event Loop
    bool done = false;
    ImVec4 clear_color = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
    
    auto last_time = std::chrono::high_resolution_clock::now();
    
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        // Perform simulation steps in real-time if Play is active
        if (g_is_running) {
            auto current_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = current_time - last_time;
            
            // Run multiple steps if simulation speed is > 1
            for (int step = 0; step < g_sim_speed; ++step) {
                StepSimulation();
            }
            last_time = current_time;
        } else {
            last_time = std::chrono::high_resolution_clock::now();
        }

        // Start ImGui Frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render our application windows
        RenderUI();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // 1 = Vsync ON
    }

    // Cleanup resources
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// --- DX11 HELPER FUNCTIONS ---
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    
    if (FAILED(hr)) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) // Disable ALT application menu shortcut
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
