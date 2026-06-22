// component_library.h
// ICE Physical Component Library for 1DSIM Desktop
// Each component defines: type ID, schematic draw function, typed ports, and internal solver model.

#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../../thirdparty/imgui/imgui.h"

// ─── Port Types ────────────────────────────────────────────────────────────────
enum class PortType {
    Coolant,      // Liquid coolant loop (blue)
    Air,          // Airflow (light grey)
    Heat,         // Conduction/radiation heat path (red)
    Oil,          // Oil circuit (amber)
    Any           // Unconstrained (for generic nodes)
};

inline const char* PortTypeName(PortType t) {
    switch (t) {
        case PortType::Coolant: return "Coolant";
        case PortType::Air:     return "Air";
        case PortType::Heat:    return "Heat";
        case PortType::Oil:     return "Oil";
        default:                return "Any";
    }
}

inline ImU32 PortTypeColor(PortType t) {
    switch (t) {
        case PortType::Coolant: return IM_COL32(59, 130, 246, 255);   // Blue
        case PortType::Air:     return IM_COL32(156, 163, 175, 255);  // Grey
        case PortType::Heat:    return IM_COL32(239, 68, 68, 255);    // Red
        case PortType::Oil:     return IM_COL32(245, 158, 11, 255);   // Amber
        default:                return IM_COL32(200, 200, 200, 255);
    }
}

inline bool PortTypesCompatible(PortType a, PortType b) {
    if (a == PortType::Any || b == PortType::Any) return true;
    return a == b;
}

// ─── Port Definition ───────────────────────────────────────────────────────────
struct ComponentPortDef {
    std::string id;        // "coolant_in", "air_out", etc.
    std::string label;
    PortType    type;
    float       dx;        // Offset from component centre (in component-local space, 100px units)
    float       dy;
    char        face;      // 'L','R','T','B'  — for orthogonal routing
    bool        isOutput;  // false=input (triangle in), true=output (triangle out)
};

// ─── Internal Node Definition ─────────────────────────────────────────────────
struct CompNodeDef {
    int         localId;          // 1-based local ID within component
    std::string role;             // "jacket", "coolant", "core", etc.
    float       tempC;            // default temperature [C]
    double      capacity;         // J/K (solid) or ignored (fluid: computed from volume)
    double      q_gen;            // W
    bool        is_fixed;
    int         domain;           // 0: Solid, 1: Fluid
    std::string fluid_medium;     // "Water", "" for solid
    double      fluid_volume;     // Liters (fluid domain only)
    // Param key for capacity (overridden by params)
    std::string capacity_param;   // e.g. "block_capacity" — maps to CompInstance.params
    std::string q_gen_param;      // e.g. "heat_rejection"
};

// ─── Internal Link Definition ─────────────────────────────────────────────────
struct CompLinkDef {
    int    localId;
    int    nodeA;          // local node IDs
    int    nodeB;
    int    type;           // 0:Cond, 1:Conv, 2:Rad, 3:Flow, 4:Fan
    std::string p1_param;  // param key for p1
    std::string p2_param;  // param key for p2 (or literal direction)
    double p1_default;
    double p2_default;
};

// Maps a port ID to the internal local node that represents it on the boundary
struct PortNodeMapping {
    std::string portId;
    int         localNodeId;
};

// ─── Parameter Default ────────────────────────────────────────────────────────
struct CompParamDef {
    std::string key;
    std::string label;
    std::string unit;
    std::string description;
    double      defaultValue;
    double      minVal;
    double      maxVal;
    double      step;
};

// ─── Component Type Definition ────────────────────────────────────────────────
enum class CompType {
    // ICE Components
    EngineBlock,
    Radiator,
    RadiatorFan,
    WaterPump,
    Thermostat,
    CoolantHose,
    OilCooler,
    HeaterCore,
    ExpansionTank,
    AmbientAir,
    // Generic (backward compatible)
    GenericNode
};

struct ComponentDef {
    CompType    type;
    std::string id;           // "engine_block", "radiator", etc.
    std::string name;         // Display name
    std::string description;
    std::string category;     // "ICE", "Common"
    ImU32       bodyColor;    // Fill colour for symbol body
    ImU32       borderColor;  // Border/outline colour
    float       width;        // Canvas footprint width [px at zoom=1]
    float       height;       // Canvas footprint height [px at zoom=1]

    std::vector<ComponentPortDef>  ports;
    std::vector<CompNodeDef>       nodes;
    std::vector<CompLinkDef>       links;
    std::vector<PortNodeMapping>   portNodeMap;
    std::vector<CompParamDef>      params;

    // Draw function — renders the P&ID schematic symbol
    // draw_list, centre (canvas-space), zoom, selected, running, temps[]
    std::function<void(ImDrawList*, ImVec2, float, bool, bool, const std::vector<double>&)> drawSymbol;
};

// ─── Component Instance ───────────────────────────────────────────────────────
struct CompInstance {
    int         instId;        // Unique instance ID (global)
    CompType    type;
    std::string defId;         // Maps to ComponentDef::id
    float       x, y;         // Canvas position (centre)
    float       rotation = 0; // 0, 90, 180, 270 degrees
    bool        selected = false;

    // Runtime param values (keyed by CompParamDef::key)
    std::unordered_map<std::string, double> params;

    // Compiled solver IDs (globalNodeId = instId*100 + localNodeId)
    // These are populated by CompileToSolver()
    std::vector<int> solverNodeIds;    // parallel to ComponentDef::nodes
    std::vector<int> solverLinkIds;    // parallel to ComponentDef::links

    // Live temperatures from solver (per internal node, indexed by localId-1)
    std::vector<double> nodeTemps;
};

// ─── Component Connection ─────────────────────────────────────────────────────
struct CompConnection {
    int         connId;
    int         fromInstId;
    std::string fromPortId;
    int         toInstId;
    std::string toPortId;
    PortType    portType;      // redundant but convenient for rendering
    // For Coolant connections — flow parameters
    double      flow_rate = 10.0;   // L/min
    // Solver link ID generated for this connection
    int         solverLinkId = -1;
};

// ─── Component Library ────────────────────────────────────────────────────────
const std::vector<ComponentDef>& GetComponentLibrary();

// Get a definition by type
const ComponentDef* GetCompDef(CompType type);
const ComponentDef* GetCompDefById(const std::string& id);

// Port world-position helpers
ImVec2 GetPortWorldPos(const CompInstance& inst, const ComponentDef& def, const std::string& portId, ImVec2 canvasOrigin, float zoom, ImVec2 scrolling);
ImVec2 GetPortCanvasPos(const CompInstance& inst, const ComponentDef& def, const std::string& portId);
void DrawPorts(ImDrawList* dl, const ComponentDef& def, ImVec2 centre, float zoom, bool selected, const std::string& hoveredPort = "");

// Compile component + connection graph into flat DesktopNode/DesktopLink vectors
// Returns false if compilation fails
struct DesktopNode;
struct DesktopLink;
bool CompileComponentGraph(
    const std::vector<CompInstance>&   instances,
    const std::vector<CompConnection>& connections,
    const std::vector<ComponentDef>&   library,
    std::vector<DesktopNode>&          outNodes,
    std::vector<DesktopLink>&          outLinks
);
