#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../../thirdparty/imgui/imgui.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846 // pi
#endif

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

    virtual int ResolveSolverNodeId(int link_type, bool is_source_node, const DesktopNode* other = nullptr) const {
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
        
        // Text is drawn by the canvas layer so long generic labels can be clipped consistently.
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
        params["flow_resistance"] = 0.1;

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

    int ResolveSolverNodeId(int link_type, bool is_source_node, const DesktopNode* other = nullptr) const override {
        if (link_type == 3 || link_type == 4) {
            if (other) {
                // If it is connected to oil gallery port, return id
                float min_d = 1e9f;
                std::string closest_port = "";
                for (const auto& p : GetPorts()) {
                    float dist = std::hypot((x + p.dx) - other->x, (y + p.dy) - other->y);
                    if (dist < min_d) {
                        min_d = dist;
                        closest_port = p.id;
                    }
                }
                if (closest_port == "oil_in") {
                    return id; // block (solid/oil gallery)
                }
            }
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
        params["flow_resistance"] = 0.05;

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

    int ResolveSolverNodeId(int link_type, bool is_source_node, const DesktopNode* other = nullptr) const override {
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

class WaterPumpNode : public DesktopNode {
public:
    void GetBounds(float& out_w, float& out_h) const override {
        out_w = 48.0f;
        out_h = 48.0f;
    }

    WaterPumpNode() {
        name = "Water Pump";
        domain = 1;
        fluid_medium = "Water";
        fluid_volume = 0.5;
        params["speed"] = 1.0;
        params["p_max"] = 100000.0;
        params["q_max"] = 50.0;
        params["flow_rate"] = 0.0;
    }

    std::shared_ptr<DesktopNode> clone() const override {
        return std::make_shared<WaterPumpNode>(*this);
    }

    std::vector<PortDef> GetPorts() const override {
        return {
            { "coolant_in",  "Inlet",  PortType::Coolant, -24, 0, 'L', false },
            { "coolant_out", "Outlet", PortType::Coolant, +24, 0, 'R', true  }
        };
    }

    void DrawSymbol(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) override {
        float R = 24 * z;
        ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(50, 200, 150, 255);
        dl->AddCircleFilled(c, R, IM_COL32(10, 35, 28, 230));
        dl->AddCircle(c, R, border, 32, sel ? 2.5f : 1.5f);

        static float pumpAngle = 0.0f;
        if (running) {
            double spd = 1.0;
            auto it = params.find("speed");
            if (it != params.end()) spd = it->second;
            pumpAngle += 0.06f * (float)spd;
        }

        ImU32 vaneCol = IM_COL32(50, 220, 160, 200);
        for (int v = 0; v < 3; ++v) {
            float a = pumpAngle + v * (float)(2.0 * M_PI / 3.0);
            ImVec2 outer(c.x + R * 0.75f * cosf(a), c.y + R * 0.75f * sinf(a));
            ImVec2 inner(c.x + R * 0.3f * cosf(a + 0.7f), c.y + R * 0.3f * sinf(a + 0.7f));
            dl->AddLine(inner, outer, vaneCol, 2.5f * z);
        }

        float ax = c.x + R + 6 * z;
        dl->AddLine(ImVec2(c.x + R * 0.7f, c.y), ImVec2(ax, c.y), border, 1.5f);
        dl->AddTriangleFilled(ImVec2(ax, c.y), ImVec2(ax - 5 * z, c.y - 3 * z), ImVec2(ax - 5 * z, c.y + 3 * z), border);

        dl->AddCircleFilled(c, 4 * z, border);

        const char* lbl = "PUMP";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, c.y + R + 4 * z), IM_COL32(50, 220, 160, 255), lbl);
    }
};

class ThermostatNode : public DesktopNode {
public:
    void GetBounds(float& out_w, float& out_h) const override {
        out_w = 56.0f;
        out_h = 56.0f;
    }

    ThermostatNode() {
        name = "Thermostat";
        domain = 1;
        fluid_medium = "Water";
        fluid_volume = 0.2;
        params["open_temp"] = 82.0;
        params["full_open"] = 95.0;
        params["flow_resistance"] = 0.05;
    }

    std::shared_ptr<DesktopNode> clone() const override {
        return std::make_shared<ThermostatNode>(*this);
    }

    std::vector<PortDef> GetPorts() const override {
        return {
            { "inlet",      "Inlet",        PortType::Coolant,  0,  -28, 'T', false },
            { "main_out",   "Main Circuit",  PortType::Coolant, +28,   0, 'R', true  },
            { "bypass_out", "Bypass",        PortType::Coolant, -28,   0, 'L', true  }
        };
    }

    void DrawSymbol(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) override {
        float S = 28 * z;

        ImVec2 top(c.x, c.y - S), bot(c.x, c.y + S);
        ImVec2 lft(c.x - S, c.y), rgt(c.x + S, c.y);

        ImU32 fill   = IM_COL32(15, 40, 25, 230);
        ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(50, 200, 100, 255);

        dl->AddQuadFilled(top, rgt, bot, lft, fill);
        dl->AddQuad(top, rgt, bot, lft, border, sel ? 2.5f : 1.5f);

        ImU32 spring = IM_COL32(80, 220, 120, 180);
        int nLines = 4;
        for (int i = 0; i <= nLines; ++i) {
            float t = (float)i / nLines;
            float yy = c.y - S * (1.0f - t * 2.0f) * 0.7f;
            float xRange = S * (1.0f - std::abs(1.0f - 2.0f * t)) * 0.6f;
            dl->AddLine(ImVec2(c.x - xRange, yy), ImVec2(c.x + xRange, yy), spring, 1.0f);
        }

        const char* lbl = "TSTAT";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, c.y + S + 4 * z), IM_COL32(80, 220, 120, 255), lbl);
    }
};

class CoolantHoseNode : public DesktopNode {
public:
    void GetBounds(float& out_w, float& out_h) const override {
        out_w = 100.0f;
        out_h = 28.0f;
    }

    CoolantHoseNode() {
        name = "Coolant Hose";
        domain = 1;
        fluid_medium = "Water";
        fluid_volume = 0.5;
        params["hose_volume"] = 0.5;
        params["heat_loss"] = 2.0;
        params["flow_resistance"] = 0.02;
    }

    std::shared_ptr<DesktopNode> clone() const override {
        return std::make_shared<CoolantHoseNode>(*this);
    }

    std::vector<PortDef> GetPorts() const override {
        return {
            { "coolant_in",  "Inlet",  PortType::Coolant, -50, 0, 'L', false },
            { "coolant_out", "Outlet", PortType::Coolant, +50, 0, 'R', true  }
        };
    }

    void DrawSymbol(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) override {
        float W = 50 * z, H = 14 * z;
        ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

        ImU32 fill   = IM_COL32(20, 40, 70, 220);
        ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(60, 120, 200, 255);

        dl->AddRectFilled(tl, br, fill, H);

        float flangeW = 5 * z;
        dl->AddRectFilled(ImVec2(tl.x, tl.y - 3 * z), ImVec2(tl.x + flangeW, br.y + 3 * z), border, 1.0f);
        dl->AddRectFilled(ImVec2(br.x - flangeW, tl.y - 3 * z), ImVec2(br.x, br.y + 3 * z), border, 1.0f);

        dl->AddRect(tl, br, border, H, 0, sel ? 2.5f : 1.2f);

        if (running) {
            static float hosePhase = 0.0f;
            hosePhase += 0.04f;
            float cx = c.x + (fmodf(hosePhase * 40.0f, W * 2.0f) - W);
            if (cx > tl.x + 8 * z && cx < br.x - 8 * z) {
                float ch = 6 * z;
                dl->AddLine(ImVec2(cx - ch, c.y - ch), ImVec2(cx, c.y), border, 1.5f);
                dl->AddLine(ImVec2(cx, c.y), ImVec2(cx - ch, c.y + ch), border, 1.5f);
            }
        }

        const char* lbl = "HOSE";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 3 * z), IM_COL32(80, 140, 220, 255), lbl);
    }
};

class OilCoolerNode : public DesktopNode {
public:
    std::shared_ptr<DesktopNode> oilNode;
    std::shared_ptr<DesktopLink> internalConv;

    void GetBounds(float& out_w, float& out_h) const override {
        out_w = 80.0f;
        out_h = 80.0f;
    }

    OilCoolerNode() {
        name = "Oil Cooler";
        domain = 1;
        fluid_medium = "Water";
        fluid_volume = 0.5;
        
        params["oil_coolant_hA"] = 400.0;
        params["oil_volume"] = 0.8;
        params["coolant_volume"] = 0.5;
        params["flow_resistance"] = 0.03;

        oilNode = std::make_shared<DesktopNode>();
        oilNode->name = "Oil Cooler (Oil)";
        oilNode->domain = 1;
        oilNode->fluid_medium = "Oil";
        oilNode->fluid_volume = 0.8;

        internalConv = std::make_shared<DesktopLink>();
        internalConv->type = 1; // Conv
        internalConv->p1 = 400.0;
    }

    std::shared_ptr<DesktopNode> clone() const override {
        auto copy = std::make_shared<OilCoolerNode>(*this);
        copy->oilNode = std::static_pointer_cast<DesktopNode>(oilNode->clone());
        copy->internalConv = std::static_pointer_cast<DesktopLink>(internalConv->clone());
        return copy;
    }

    std::vector<DesktopNode*> GetSolverNodes() override {
        return { this, oilNode.get() };
    }

    std::vector<std::shared_ptr<DesktopLink>> GetInternalLinks() override {
        return { internalConv };
    }

    int ResolveSolverNodeId(int link_type, bool is_source_node, const DesktopNode* other = nullptr) const override {
        if (!other) return id;
        float min_d = 1e9f;
        std::string closest_port = "";
        for (const auto& p : GetPorts()) {
            float dist = std::hypot((x + p.dx) - other->x, (y + p.dy) - other->y);
            if (dist < min_d) {
                min_d = dist;
                closest_port = p.id;
            }
        }
        if (closest_port == "oil_in" || closest_port == "oil_out") {
            return oilNode->id;
        }
        return id;
    }

    std::vector<PortDef> GetPorts() const override {
        return {
            { "oil_in",      "Oil In",      PortType::Oil,     -40, -20, 'L', false },
            { "oil_out",     "Oil Out",     PortType::Oil,     -40, +20, 'L', true  },
            { "coolant_in",  "Coolant In",  PortType::Coolant, +40, -20, 'R', false },
            { "coolant_out", "Coolant Out", PortType::Coolant, +40, +20, 'R', true  }
        };
    }

    void DrawSymbol(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) override {
        float W = 40 * z, H = 40 * z;
        ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

        dl->AddRectFilled(tl, br, IM_COL32(35, 20, 5, 230), 3.0f);

        int nCoils = 5;
        float coilStep = (H * 2.0f) / (nCoils + 1);
        for (int i = 1; i <= nCoils; ++i) {
            float y = tl.y + i * coilStep;
            ImU32 coilColor = IM_COL32(200, 130, 30, 160);
            float amp = 6 * z;
            dl->AddLine(ImVec2(tl.x + 5 * z, y), ImVec2(tl.x + 5 * z + 10 * z, y - amp), coilColor, 1.5f);
            dl->AddLine(ImVec2(tl.x + 15 * z, y - amp), ImVec2(tl.x + 15 * z + 20 * z, y + amp), coilColor, 1.5f);
            dl->AddLine(ImVec2(tl.x + 35 * z, y + amp), ImVec2(br.x - 5 * z, y), coilColor, 1.5f);
        }

        ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(200, 130, 30, 255);
        dl->AddRect(tl, br, border, 3.0f, 0, sel ? 2.5f : 1.5f);

        const char* lbl = "OIL COOLER";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 4 * z), IM_COL32(220, 150, 40, 255), lbl);
    }
};

class HeaterCoreNode : public DesktopNode {
public:
    std::shared_ptr<DesktopNode> coreNode;
    std::shared_ptr<DesktopLink> internalConv;

    void GetBounds(float& out_w, float& out_h) const override {
        out_w = 45.0f;
        out_h = 28.0f;
    }

    HeaterCoreNode() {
        name = "Heater Core";
        domain = 1;
        fluid_medium = "Water";
        fluid_volume = 0.6;
        
        params["coolant_hA"] = 300.0;
        params["core_capacity"] = 800.0;
        params["coolant_volume"] = 0.6;
        params["flow_resistance"] = 0.04;

        coreNode = std::make_shared<DesktopNode>();
        coreNode->name = "Heater Core (Core)";
        coreNode->domain = 0;
        coreNode->capacity = 800.0;

        internalConv = std::make_shared<DesktopLink>();
        internalConv->type = 1; // Conv
        internalConv->p1 = 300.0;
    }

    std::shared_ptr<DesktopNode> clone() const override {
        auto copy = std::make_shared<HeaterCoreNode>(*this);
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

    int ResolveSolverNodeId(int link_type, bool is_source_node, const DesktopNode* other = nullptr) const override {
        if (link_type == 3 || link_type == 4) {
            return id;
        }
        return coreNode->id;
    }

    std::vector<PortDef> GetPorts() const override {
        return {
            { "coolant_in",  "Coolant In",  PortType::Coolant, -45, 0, 'L', false },
            { "coolant_out", "Outlet",      PortType::Coolant, +45, 0, 'R', true  },
            { "air_in",      "Cabin Air In",PortType::Air,       0,-28, 'T', false },
            { "air_out",     "Warm Air Out",PortType::Air,       0,+28, 'B', true  }
        };
    }

    void DrawSymbol(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) override {
        float W = 45 * z, H = 28 * z;
        ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

        dl->AddRectFilled(tl, br, IM_COL32(40, 10, 10, 230), 3.0f);

        int nFins = 7;
        float finStep = (H * 2.0f) / (nFins + 1);
        for (int i = 1; i <= nFins; ++i) {
            float fy = tl.y + i * finStep;
            dl->AddLine(ImVec2(tl.x + 4 * z, fy), ImVec2(br.x - 4 * z, fy),
                        IM_COL32(220, 80, 80, 160), 1.0f);
        }

        float hdrW = 7 * z;
        dl->AddRectFilled(tl, ImVec2(tl.x + hdrW, br.y), IM_COL32(160, 50, 50, 200));
        dl->AddRectFilled(ImVec2(br.x - hdrW, tl.y), br, IM_COL32(160, 50, 50, 200));

        ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(220, 80, 80, 255);
        dl->AddRect(tl, br, border, 3.0f, 0, sel ? 2.5f : 1.5f);

        const char* lbl = "HTR CORE";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 4 * z), IM_COL32(220, 100, 100, 255), lbl);
    }
};

class ExpansionTankNode : public DesktopNode {
public:
    void GetBounds(float& out_w, float& out_h) const override {
        out_w = 22.0f;
        out_h = 35.0f;
    }

    ExpansionTankNode() {
        name = "Expansion Tank";
        domain = 1;
        fluid_medium = "Water";
        fluid_volume = 1.5;
        params["tank_volume"] = 1.5;
    }

    std::shared_ptr<DesktopNode> clone() const override {
        return std::make_shared<ExpansionTankNode>(*this);
    }

    std::vector<PortDef> GetPorts() const override {
        return {
            { "coolant_port", "Coolant", PortType::Coolant, 0, +35, 'B', false }
        };
    }

    void DrawSymbol(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) override {
        float W = 22 * z, H = 35 * z;
        ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

        dl->AddRectFilled(tl, br, IM_COL32(15, 35, 60, 230), 6.0f);

        float levelY = tl.y + (H * 2.0f) * 0.3f;
        dl->AddRectFilled(ImVec2(tl.x + 2 * z, levelY), ImVec2(br.x - 2 * z, br.y - 4 * z),
                          IM_COL32(40, 90, 160, 180), 4.0f);

        dl->AddLine(ImVec2(tl.x + 3 * z, levelY), ImVec2(br.x - 3 * z, levelY),
                    IM_COL32(80, 160, 220, 220), 1.5f);

        dl->AddRectFilled(ImVec2(c.x - 8 * z, tl.y - 5 * z), ImVec2(c.x + 8 * z, tl.y + 1 * z),
                          IM_COL32(60, 80, 110, 220), 3.0f);

        ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(60, 130, 210, 255);
        dl->AddRect(tl, br, border, 6.0f, 0, sel ? 2.5f : 1.5f);

        const char* lbl = "EXP TANK";
        ImVec2 ls = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 4 * z), IM_COL32(80, 150, 220, 255), lbl);
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
