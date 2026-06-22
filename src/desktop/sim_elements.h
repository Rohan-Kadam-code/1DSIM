#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../../thirdparty/imgui/imgui.h"

// Forward declaration of ThermalSystem (from solver.h)
class ThermalSystem;

enum class PortType {
    Coolant, Air, Heat, Oil, Any
};

inline ImU32 PortTypeColor(PortType t) {
    switch (t) {
        case PortType::Coolant: return IM_COL32(59, 130, 246, 255);   // Blue
        case PortType::Air:     return IM_COL32(156, 163, 175, 255);  // Grey
        case PortType::Heat:    return IM_COL32(239, 68, 68, 255);    // Red
        case PortType::Oil:     return IM_COL32(245, 158, 11, 255);   // Amber
        default:                return IM_COL32(200, 200, 200, 255);
    }
}

struct PortDef {
    std::string id;
    std::string label;
    PortType type;
    float dx, dy;
    char face;
    bool isOutput;
};

// ─── DesktopLink ─────────────────────────────────────────────────────────────
class DesktopLink {
public:
    int id = 0;
    int node_a = 0;
    int node_b = 0;
    int type = 0;          // 0:Cond, 1:Conv, 2:Rad, 3:Flow, 4:Fan
    double p1 = 10.0;         // Parameter
    double p2 = 1.0;         // Direction indicator / v_max
    double g_a1 = 0.0;
    double g_a2 = 0.0;
    double fan_area = 0.005;

    virtual ~DesktopLink() = default;

    virtual std::shared_ptr<DesktopLink> clone() const {
        return std::make_shared<DesktopLink>(*this);
    }

    // By default, just returns itself as the solver link
    virtual std::vector<DesktopLink*> GetSolverLinks() {
        return { this };
    }
};

// ─── DesktopNode ─────────────────────────────────────────────────────────────
class DesktopNode {
public:
    int id = 0;
    std::string name = "Mass";
    float x = 0.0f;
    float y = 0.0f;
    double temp = 25.0;       // in Celsius
    double capacity = 1000.0;   // J/K
    double q_gen = 0.0;      // W
    bool is_fixed = false;
    double temp_init = 25.0; // Initial temperature tracker
    double c_a1 = 0.0;
    double c_a2 = 0.0;
    std::string material = "Custom";
    double mass = 1.0; // kg
    int domain = 0; // 0: Solid, 1: Fluid
    std::string fluid_medium = "Water";
    double fluid_volume = 1.0; // Liters
    double fluid_mix_ratio = 0.5; // 0.0–1.0 glycol fraction
    double fluid_rho_a0 = 1000.0, fluid_rho_a1 = 0.0, fluid_rho_a2 = 0.0;
    double fluid_cp_a0 = 4184.0, fluid_cp_a1 = 0.0, fluid_cp_a2 = 0.0;

    // Component-specific parameters (moved from CompInstance)
    std::unordered_map<std::string, double> params;
    float rotation = 0;

    virtual ~DesktopNode() = default;

    virtual std::shared_ptr<DesktopNode> clone() const {
        return std::make_shared<DesktopNode>(*this);
    }

    virtual void GetBounds(float& out_w, float& out_h) const {
        out_w = 70.0f;
        out_h = 40.0f;
    }

    virtual int ResolveSolverNodeId(int link_type, bool is_source_node) const {
        return id;
    }

    // Sub-nodes for composite components (like EngineBlock having block+jacket)
    virtual std::vector<DesktopNode*> GetSolverNodes() {
        return { this };
    }

    // Internal links for composite components
    virtual std::vector<std::shared_ptr<DesktopLink>> GetInternalLinks() {
        return {};
    }

    virtual std::vector<PortDef> GetPorts() const {
        return {};
    }

    virtual void DrawSymbol(ImDrawList* dl, ImVec2 canvas_pos, float zoom, bool selected, bool running, const std::vector<double>& temps) {
        // Generic draw (overridden by specific objects)
        float w = 70 * zoom;
        float h = 40 * zoom;
        ImVec2 tl(canvas_pos.x - w / 2, canvas_pos.y - h / 2);
        ImVec2 br(canvas_pos.x + w / 2, canvas_pos.y + h / 2);
        ImU32 col = selected ? IM_COL32(255, 200, 50, 255) : IM_COL32(100, 150, 200, 255);
        dl->AddRectFilled(tl, br, IM_COL32(30, 40, 50, 230), 4.0f);
        dl->AddRect(tl, br, col, 4.0f, 0, selected ? 2.5f : 1.5f);
        
        ImVec2 ts = ImGui::CalcTextSize(name.c_str());
        dl->AddText(ImVec2(canvas_pos.x - ts.x/2, canvas_pos.y - ts.y/2), IM_COL32(255,255,255,255), name.c_str());
    }
};

// ─── Specific Components ─────────────────────────────────────────────────────
class EngineBlockNode : public DesktopNode {
public:
    std::shared_ptr<DesktopNode> jacketNode;
    std::shared_ptr<DesktopLink> internalCond;

    void GetBounds(float& out_w, float& out_h) const override {
        out_w = 140.0f;
        out_h = 100.0f;
    }

    EngineBlockNode() {
        name = "Engine Block";
        capacity = 55000.0;
        q_gen = 15000.0;
        params["heat_rejection"] = 15000.0;
        params["block_capacity"] = 55000.0;
        params["block_jacket_cond"] = 1200.0;
        params["jacket_volume"] = 4.5;

        // Sub-node setup
        jacketNode = std::make_shared<DesktopNode>();
        jacketNode->name = "Jacket";
        jacketNode->domain = 1;
        jacketNode->fluid_medium = "Water";

        internalCond = std::make_shared<DesktopLink>();
        internalCond->type = 0; // Cond
    }

    std::shared_ptr<DesktopNode> clone() const override {
        auto copy = std::make_shared<EngineBlockNode>(*this);
        // Deep copy sub-nodes
        copy->jacketNode = std::static_pointer_cast<DesktopNode>(jacketNode->clone());
        copy->internalCond = std::static_pointer_cast<DesktopLink>(internalCond->clone());
        return copy;
    }

    std::vector<DesktopNode*> GetSolverNodes() override {
        // ID mapping needs to be handled dynamically when syncing with solver
        return { this, jacketNode.get() };
    }

    std::vector<std::shared_ptr<DesktopLink>> GetInternalLinks() override {
        return { internalCond };
    }

    int ResolveSolverNodeId(int link_type, bool is_source_node) const override {
        if (link_type == 3 || link_type == 4) {
            return jacketNode->id;
        }
        return id;
    }

    std::vector<PortDef> GetPorts() const override {
        return {
            { "coolant_out", "Coolant Out", PortType::Coolant, +70,    0,  'R', true  },
            { "coolant_in",  "Coolant In",  PortType::Coolant, -70,    0,  'L', false },
            { "heat_out",    "Block Heat",  PortType::Heat,     0,  -50,  'T', true  },
            { "oil_in",      "Oil Gallery", PortType::Oil,      0,  +50,  'B', false }
        };
    }

    void DrawSymbol(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) override {
        float W = 70 * z, H = 50 * z;
        ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

        ImU32 bodyFill = temps.empty() ? IM_COL32(40, 25, 10, 240) : IM_COL32(200, 50, 50, 255); // simplified TempToColor
        dl->AddRectFilled(tl, br, IM_COL32(40, 25, 10, 240), 4.0f);

        ImU32 hatch = IM_COL32(180, 100, 40, 120);
        float step = 16.0f * z;
        for (float x = tl.x; x < br.x + H * 2; x += step) {
            ImVec2 a(x, tl.y), b(x - H * 2, br.y);
            float ax = std::max(a.x, tl.x); float ay = tl.y + (ax - a.x);
            float bx = std::min(b.x, br.x); float by = br.y - (b.x - bx);
            if (ax < br.x && bx > tl.x)
                dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), hatch, 1.2f);
        }

        float boreR = 10 * z;
        float boreY = c.y - H * 0.4f;
        for (int i = -1; i <= 1; ++i) {
            ImVec2 bc(c.x + i * (W * 0.5f), boreY);
            dl->AddCircleFilled(bc, boreR, IM_COL32(20, 12, 5, 220));
            dl->AddCircle(bc, boreR, IM_COL32(220, 160, 60, 200), 16, 1.5f);
            dl->AddLine(ImVec2(bc.x - boreR * 0.7f, bc.y), ImVec2(bc.x + boreR * 0.7f, bc.y),
                        IM_COL32(220, 160, 60, 200), 2.0f);
        }

        ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(220, 140, 50, 255);
        dl->AddRect(tl, br, border, 4.0f, 0, sel ? 2.5f : 1.5f);

        const char* lbl = "ENGINE";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 4 * z), IM_COL32(220, 160, 50, 255), lbl);
    }
};

class RadiatorNode : public DesktopNode {
public:
    std::shared_ptr<DesktopNode> coreNode;
    std::shared_ptr<DesktopLink> internalConv;

    void GetBounds(float& out_w, float& out_h) const override {
        out_w = 60.0f;
        out_h = 120.0f;
    }

    RadiatorNode() {
        name = "Radiator";
        domain = 1;
        fluid_medium = "Water";
        params["coolant_hA"] = 800.0;
        params["air_hA"] = 500.0;
        params["coolant_volume"] = 3.0;
        params["core_capacity"] = 8000.0;

        coreNode = std::make_shared<DesktopNode>();
        coreNode->name = "Radiator Core";

        internalConv = std::make_shared<DesktopLink>();
        internalConv->type = 1; // Conv
    }

    std::shared_ptr<DesktopNode> clone() const override {
        auto copy = std::make_shared<RadiatorNode>(*this);
        copy->coreNode = std::static_pointer_cast<DesktopNode>(coreNode->clone());
        copy->internalConv = std::static_pointer_cast<DesktopLink>(internalConv->clone());
        return copy;
    }

    std::vector<DesktopNode*> GetSolverNodes() override {
        return { this, coreNode.get() };
    }

    std::vector<std::shared_ptr<DesktopLink>> GetInternalLinks() override {
        return { internalConv };
    }

    int ResolveSolverNodeId(int link_type, bool is_source_node) const override {
        if (link_type == 3 || link_type == 4) {
            return id;
        }
        return coreNode->id;
    }

    std::vector<PortDef> GetPorts() const override {
        return {
            { "coolant_in",  "Coolant In",  PortType::Coolant,  0, -60, 'T', false },
            { "coolant_out", "Coolant Out", PortType::Coolant,  0, +60, 'B', true  },
            { "air_in",      "Air In",      PortType::Air,    -30,   0, 'L', false },
            { "air_out",     "Air Out",     PortType::Air,    +30,   0, 'R', true  }
        };
    }

    void DrawSymbol(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) override {
        float W = 30 * z, H = 60 * z;
        ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

        dl->AddRectFilled(tl, br, IM_COL32(15, 30, 60, 230), 3.0f);
        
        int numFins = 9;
        float finStep = (br.x - tl.x) / (numFins + 1);
        for (int i = 1; i <= numFins; ++i) {
            float fx = tl.x + i * finStep;
            dl->AddLine(ImVec2(fx, tl.y + 4 * z), ImVec2(fx, br.y - 4 * z), IM_COL32(80, 140, 220, 180), 1.0f);
        }

        float hdrH = 8 * z;
        dl->AddRectFilled(tl, ImVec2(br.x, tl.y + hdrH), IM_COL32(40, 90, 160, 200), 0);
        dl->AddRectFilled(ImVec2(tl.x, br.y - hdrH), br, IM_COL32(40, 90, 160, 200), 0);

        ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(60, 130, 220, 255);
        dl->AddRect(tl, br, border, 3.0f, 0, sel ? 2.5f : 1.5f);

        const char* lbl = "RAD";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 4 * z), IM_COL32(80, 160, 255, 255), lbl);
    }
};

class AmbientAirNode : public DesktopNode {
public:
    void GetBounds(float& out_w, float& out_h) const override {
        out_w = 60.0f;
        out_h = 40.0f;
    }

    AmbientAirNode() {
        name = "Ambient Air";
        is_fixed = true;
        temp = 25.0;
        params["temp_c"] = 25.0;
    }

    std::shared_ptr<DesktopNode> clone() const override {
        return std::make_shared<AmbientAirNode>(*this);
    }

    std::vector<PortDef> GetPorts() const override {
        return { { "air_port", "Air Connection", PortType::Air, 0, -25, 'T', false } };
    }

    void DrawSymbol(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) override {
        float W = 30 * z;
        ImU32 col = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(140, 200, 140, 255);

        dl->AddLine(ImVec2(c.x - W, c.y), ImVec2(c.x + W, c.y), col, 2.0f);
        dl->AddLine(ImVec2(c.x - W * 0.7f, c.y + 6 * z), ImVec2(c.x + W * 0.7f, c.y + 6 * z), col, 2.0f);
        dl->AddLine(ImVec2(c.x - W * 0.35f, c.y + 12 * z), ImVec2(c.x + W * 0.35f, c.y + 12 * z), col, 2.0f);
        dl->AddLine(ImVec2(c.x, c.y - 20 * z), ImVec2(c.x, c.y), col, 1.5f);

        const char* lbl = "AMBIENT";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, c.y + 18 * z), IM_COL32(140, 200, 140, 255), lbl);
    }
};

// ─── Specific Links ──────────────────────────────────────────────────────────
class RadiatorFanLink : public DesktopLink {
public:
    RadiatorFanLink() {
        type = 4; // Fan
        p1 = 200.0;
        p2 = 12.0;
        g_a1 = 0.5;
        g_a2 = 2.0;
    }

    std::shared_ptr<DesktopLink> clone() const override {
        return std::make_shared<RadiatorFanLink>(*this);
    }
};

class WaterPumpLink : public DesktopLink {
public:
    WaterPumpLink() {
        type = 3; // Flow
        p1 = 20.0;
        p2 = 1.0;
    }

    std::shared_ptr<DesktopLink> clone() const override {
        return std::make_shared<WaterPumpLink>(*this);
    }
};
