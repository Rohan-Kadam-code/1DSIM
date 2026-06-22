// component_library.cpp
// ICE Physical Component Library — P&ID schematic symbols + solver model mappings

#define NOMINMAX
#include "component_library.h"
#include "../core/solver.h"  // for DesktopNode/DesktopLink forward declarations

// ─── Include DesktopNode/DesktopLink (defined in main.cpp, declared here) ─────
// We use a minimal re-declaration to avoid circular includes.
// The full struct is in main.cpp; we only need the fields we write.
#include <cmath>
#include <algorithm>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── DesktopNode / DesktopLink minimal declarations (match main.cpp exactly) ──
struct DesktopNode {
    int id;
    std::string name;
    float x, y;
    double temp;
    double capacity;
    double q_gen;
    bool is_fixed;
    double temp_init = 25.0;
    double c_a1 = 0.0, c_a2 = 0.0;
    std::string material = "Custom";
    double mass = 1.0;
    int domain = 0;
    std::string fluid_medium = "Water";
    double fluid_volume = 1.0;
    double fluid_mix_ratio = 0.5;
    double fluid_rho_a0 = 1000.0, fluid_rho_a1 = 0.0, fluid_rho_a2 = 0.0;
    double fluid_cp_a0 = 4184.0, fluid_cp_a1 = 0.0, fluid_cp_a2 = 0.0;
};

struct DesktopLink {
    int id, node_a, node_b, type;
    double p1, p2;
    double g_a1 = 0.0, g_a2 = 0.0;
    double fan_area = 0.005;
};

// ─── DRAWING HELPERS ──────────────────────────────────────────────────────────

static ImU32 TempToColor(double tempC) {
    // Blue (cold) to Red (hot) over 0–120°C
    float t = (float)std::max(0.0, std::min(1.0, (tempC - 0.0) / 120.0));
    return IM_COL32(
        (int)(t * 220 + 30),
        (int)((1.0f - std::abs(2.0f * t - 1.0f)) * 180),
        (int)((1.0f - t) * 220 + 30),
        255
    );
}

// Draw a single temperature readout label above a position
static void DrawTempLabel(ImDrawList* dl, ImVec2 pos, double tempC) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f°C", tempC);
    ImVec2 sz = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(pos.x - sz.x * 0.5f, pos.y), IM_COL32(220, 220, 220, 200), buf);
}

// Draw port indicator circles around the component
void DrawPorts(ImDrawList* dl,
                      const ComponentDef& def,
                      ImVec2 centre,
                      float zoom,
                      bool selected,
                      const std::string& hoveredPort) {
    for (const auto& p : def.ports) {
        ImVec2 pp(centre.x + p.dx * zoom, centre.y + p.dy * zoom);
        float r = 5.0f * zoom;
        ImU32 col  = PortTypeColor(p.type);
        ImU32 fill = (selected || p.id == hoveredPort)
                     ? col
                     : IM_COL32(20, 20, 30, 220);
        dl->AddCircleFilled(pp, r, fill);
        dl->AddCircle(pp, r, col, 12, 1.5f);
    }
}

// ─── P&ID SYMBOL DRAWING FUNCTIONS ────────────────────────────────────────────

// Engine Block — Rectangle with diagonal hatching inside + piston outlines
static void DrawEngine(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float W = 70 * z, H = 50 * z;
    ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

    // Body fill — orange-red tint
    ImU32 bodyFill = temps.empty() ? IM_COL32(40, 25, 10, 240) : TempToColor(temps[0]);
    dl->AddRectFilled(tl, br, IM_COL32(40, 25, 10, 240), 4.0f);
    if (!temps.empty()) {
        // Temperature heat map tinted overlay
        ImU32 heatTint = TempToColor(temps[0]);
        dl->AddRectFilled(tl, br, (heatTint & 0x00FFFFFF) | 0x55000000, 4.0f);
    }

    // Diagonal hatch lines (P&ID solid body convention)
    ImU32 hatch = IM_COL32(180, 100, 40, 120);
    float step = 16.0f * z;
    for (float x = tl.x; x < br.x + H * 2; x += step) {
        ImVec2 a(x, tl.y), b(x - H * 2, br.y);
        // Clip to rect
        float ax = std::max(a.x, tl.x); float ay = tl.y + (ax - a.x);
        float bx = std::min(b.x, br.x); float by = br.y - (b.x - bx);
        if (ax < br.x && bx > tl.x)
            dl->AddLine(ImVec2(ax, ay), ImVec2(bx, by), hatch, 1.2f);
    }

    // Piston bores (3 circles across top half)
    float boreR = 10 * z;
    float boreY = c.y - H * 0.4f;
    for (int i = -1; i <= 1; ++i) {
        ImVec2 bc(c.x + i * (W * 0.5f), boreY);
        dl->AddCircleFilled(bc, boreR, IM_COL32(20, 12, 5, 220));
        dl->AddCircle(bc, boreR, IM_COL32(220, 160, 60, 200), 16, 1.5f);
        // Piston crown
        dl->AddLine(ImVec2(bc.x - boreR * 0.7f, bc.y), ImVec2(bc.x + boreR * 0.7f, bc.y),
                    IM_COL32(220, 160, 60, 200), 2.0f);
    }

    // Border
    ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(220, 140, 50, 255);
    dl->AddRect(tl, br, border, 4.0f, 0, sel ? 2.5f : 1.5f);

    // Label
    const char* lbl = "ENGINE";
    ImVec2 ls = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 4 * z), IM_COL32(220, 160, 50, 255), lbl);

    if (!temps.empty()) DrawTempLabel(dl, ImVec2(c.x, tl.y - 14 * z), temps[0]);
}

// Radiator — Rectangle with vertical fin lines inside
static void DrawRadiator(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float W = 30 * z, H = 60 * z;
    ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

    // Body
    dl->AddRectFilled(tl, br, IM_COL32(15, 30, 60, 230), 3.0f);
    if (!temps.empty()) {
        ImU32 tint = (TempToColor(temps[0]) & 0x00FFFFFF) | 0x44000000;
        dl->AddRectFilled(tl, br, tint, 3.0f);
    }

    // Vertical fin lines
    int numFins = 9;
    float finStep = (br.x - tl.x) / (numFins + 1);
    for (int i = 1; i <= numFins; ++i) {
        float fx = tl.x + i * finStep;
        dl->AddLine(ImVec2(fx, tl.y + 4 * z), ImVec2(fx, br.y - 4 * z),
                    IM_COL32(80, 140, 220, 180), 1.0f);
    }

    // Coolant header boxes (top and bottom)
    float hdrH = 8 * z;
    dl->AddRectFilled(tl, ImVec2(br.x, tl.y + hdrH), IM_COL32(40, 90, 160, 200), 0);
    dl->AddRectFilled(ImVec2(tl.x, br.y - hdrH), br, IM_COL32(40, 90, 160, 200), 0);

    // Border
    ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(60, 130, 220, 255);
    dl->AddRect(tl, br, border, 3.0f, 0, sel ? 2.5f : 1.5f);

    // Label
    const char* lbl = "RAD";
    ImVec2 ls = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 4 * z), IM_COL32(80, 160, 255, 255), lbl);

    if (!temps.empty()) DrawTempLabel(dl, ImVec2(c.x, tl.y - 14 * z), temps[0]);
}

// Fan — Circle with 4 curved blades (P&ID fan symbol)
static void DrawFan(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float R = 32 * z;
    // Outer circle
    ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(160, 90, 220, 255);
    dl->AddCircleFilled(c, R, IM_COL32(25, 12, 40, 230));
    dl->AddCircle(c, R, border, 32, sel ? 2.5f : 1.5f);

    // Blades — 4 arcs at 90° each, representing fan blade sweeps
    static float bladeAngle = 0.0f;
    if (running) bladeAngle += 0.08f;

    ImU32 bladeCol = IM_COL32(180, 100, 255, 220);
    for (int b = 0; b < 4; ++b) {
        float baseAngle = bladeAngle + b * (float)(M_PI / 2.0);
        // Draw a simple curved blade as a filled triangle + arc approximation
        ImVec2 tip(c.x + R * 0.85f * cosf(baseAngle), c.y + R * 0.85f * sinf(baseAngle));
        ImVec2 mid(c.x + R * 0.55f * cosf(baseAngle + 0.6f), c.y + R * 0.55f * sinf(baseAngle + 0.6f));
        ImVec2 hub(c.x + R * 0.18f * cosf(baseAngle + 0.4f), c.y + R * 0.18f * sinf(baseAngle + 0.4f));
        dl->AddTriangleFilled(tip, mid, hub, bladeCol);
        dl->AddTriangle(tip, mid, hub, IM_COL32(210, 140, 255, 180), 1.0f);
    }

    // Hub circle
    dl->AddCircleFilled(c, 5 * z, IM_COL32(200, 140, 255, 230));

    // Label
    const char* lbl = "FAN";
    ImVec2 ls = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(c.x - ls.x * 0.5f, c.y + R + 4 * z), IM_COL32(180, 100, 255, 255), lbl);
}

// Water Pump — Circle with impeller arrow (centrifugal pump P&ID symbol)
static void DrawPump(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float R = 24 * z;
    ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(50, 200, 150, 255);
    dl->AddCircleFilled(c, R, IM_COL32(10, 35, 28, 230));
    dl->AddCircle(c, R, border, 32, sel ? 2.5f : 1.5f);

    // Impeller — 3 curved vanes
    static float pumpAngle = 0.0f;
    if (running) pumpAngle += 0.06f;

    ImU32 vaneCol = IM_COL32(50, 220, 160, 200);
    for (int v = 0; v < 3; ++v) {
        float a = pumpAngle + v * (float)(2.0 * M_PI / 3.0);
        ImVec2 outer(c.x + R * 0.75f * cosf(a), c.y + R * 0.75f * sinf(a));
        ImVec2 inner(c.x + R * 0.3f * cosf(a + 0.7f), c.y + R * 0.3f * sinf(a + 0.7f));
        dl->AddLine(inner, outer, vaneCol, 2.5f * z);
    }

    // Arrow head indicating discharge direction (right)
    float ax = c.x + R + 6 * z;
    dl->AddLine(ImVec2(c.x + R * 0.7f, c.y), ImVec2(ax, c.y), border, 1.5f);
    dl->AddTriangleFilled(ImVec2(ax, c.y), ImVec2(ax - 5 * z, c.y - 3 * z), ImVec2(ax - 5 * z, c.y + 3 * z), border);

    // Hub
    dl->AddCircleFilled(c, 4 * z, border);

    // Label
    const char* lbl = "PUMP";
    ImVec2 ls = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(c.x - ls.x * 0.5f, c.y + R + 4 * z), IM_COL32(50, 220, 160, 255), lbl);
}

// Thermostat — Diamond/rhombus with temperature lines inside (P&ID valve symbol)
static void DrawThermostat(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float S = 28 * z; // half-size of diamond

    ImVec2 top(c.x, c.y - S), bot(c.x, c.y + S);
    ImVec2 lft(c.x - S, c.y), rgt(c.x + S, c.y);

    ImU32 fill   = IM_COL32(15, 40, 25, 230);
    ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(50, 200, 100, 255);

    dl->AddQuadFilled(top, rgt, bot, lft, fill);
    dl->AddQuad(top, rgt, bot, lft, border, sel ? 2.5f : 1.5f);

    // Internal temperature spring lines
    ImU32 spring = IM_COL32(80, 220, 120, 180);
    int nLines = 4;
    for (int i = 0; i <= nLines; ++i) {
        float t = (float)i / nLines;
        float yy = c.y - S * (1.0f - t * 2.0f) * 0.7f;
        float xRange = S * (1.0f - std::abs(1.0f - 2.0f * t)) * 0.6f;
        dl->AddLine(ImVec2(c.x - xRange, yy), ImVec2(c.x + xRange, yy), spring, 1.0f);
    }

    // "T" label
    const char* lbl = "TSTAT";
    ImVec2 ls = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(c.x - ls.x * 0.5f, c.y + S + 4 * z), IM_COL32(80, 220, 120, 255), lbl);

    if (!temps.empty()) DrawTempLabel(dl, ImVec2(c.x, top.y - 14 * z), temps[0]);
}

// Hose / Pipe — Thick line with end flanges and fluid fill tint
static void DrawHose(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float W = 50 * z, H = 14 * z;
    ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

    ImU32 fill   = IM_COL32(20, 40, 70, 220);
    ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(60, 120, 200, 255);

    dl->AddRectFilled(tl, br, fill, H);
    if (!temps.empty()) {
        ImU32 tint = (TempToColor(temps[0]) & 0x00FFFFFF) | 0x44000000;
        dl->AddRectFilled(tl, br, tint, H);
    }

    // Flange ends
    float flangeW = 5 * z;
    dl->AddRectFilled(ImVec2(tl.x, tl.y - 3 * z), ImVec2(tl.x + flangeW, br.y + 3 * z), border, 1.0f);
    dl->AddRectFilled(ImVec2(br.x - flangeW, tl.y - 3 * z), ImVec2(br.x, br.y + 3 * z), border, 1.0f);

    dl->AddRect(tl, br, border, H, 0, sel ? 2.5f : 1.2f);

    // Flow chevron if running
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

    if (!temps.empty()) DrawTempLabel(dl, ImVec2(c.x, tl.y - 14 * z), temps[0]);
}

// Oil Cooler — Rectangle with coil lines inside
static void DrawOilCooler(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float W = 40 * z, H = 40 * z;
    ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

    dl->AddRectFilled(tl, br, IM_COL32(35, 20, 5, 230), 3.0f);

    // Coil lines (5 horizontal wavy arcs)
    int nCoils = 5;
    float coilStep = (H * 2.0f) / (nCoils + 1);
    for (int i = 1; i <= nCoils; ++i) {
        float y = tl.y + i * coilStep;
        ImU32 coilColor = IM_COL32(200, 130, 30, 160);
        // simple 3-segment zig-zag approximation
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

    if (!temps.empty()) DrawTempLabel(dl, ImVec2(c.x, tl.y - 14 * z), temps[0]);
}

// Heater Core — Small radiator with wavy lines (horizontal orientation)
static void DrawHeaterCore(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float W = 45 * z, H = 28 * z;
    ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

    dl->AddRectFilled(tl, br, IM_COL32(40, 10, 10, 230), 3.0f);

    // Horizontal fin lines
    int nFins = 7;
    float finStep = (H * 2.0f) / (nFins + 1);
    for (int i = 1; i <= nFins; ++i) {
        float fy = tl.y + i * finStep;
        dl->AddLine(ImVec2(tl.x + 4 * z, fy), ImVec2(br.x - 4 * z, fy),
                    IM_COL32(220, 80, 80, 160), 1.0f);
    }

    // Header boxes (left and right)
    float hdrW = 7 * z;
    dl->AddRectFilled(tl, ImVec2(tl.x + hdrW, br.y), IM_COL32(160, 50, 50, 200));
    dl->AddRectFilled(ImVec2(br.x - hdrW, tl.y), br, IM_COL32(160, 50, 50, 200));

    ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(220, 80, 80, 255);
    dl->AddRect(tl, br, border, 3.0f, 0, sel ? 2.5f : 1.5f);

    const char* lbl = "HTR CORE";
    ImVec2 ls = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 4 * z), IM_COL32(220, 100, 100, 255), lbl);

    if (!temps.empty()) DrawTempLabel(dl, ImVec2(c.x, tl.y - 14 * z), temps[0]);
}

// Expansion Tank — Rounded cylinder / reservoir symbol
static void DrawExpansionTank(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float W = 22 * z, H = 35 * z;
    ImVec2 tl(c.x - W, c.y - H), br(c.x + W, c.y + H);

    dl->AddRectFilled(tl, br, IM_COL32(15, 35, 60, 230), 6.0f);

    // Liquid level line (70% full)
    float levelY = tl.y + (H * 2.0f) * 0.3f;
    dl->AddRectFilled(ImVec2(tl.x + 2 * z, levelY), ImVec2(br.x - 2 * z, br.y - 4 * z),
                      IM_COL32(40, 90, 160, 180), 4.0f);

    // Level line
    dl->AddLine(ImVec2(tl.x + 3 * z, levelY), ImVec2(br.x - 3 * z, levelY),
                IM_COL32(80, 160, 220, 220), 1.5f);

    // Cap/vent at top
    dl->AddRectFilled(ImVec2(c.x - 8 * z, tl.y - 5 * z), ImVec2(c.x + 8 * z, tl.y + 1 * z),
                      IM_COL32(60, 80, 110, 220), 3.0f);

    ImU32 border = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(60, 130, 210, 255);
    dl->AddRect(tl, br, border, 6.0f, 0, sel ? 2.5f : 1.5f);

    const char* lbl = "EXP TANK";
    ImVec2 ls = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(c.x - ls.x * 0.5f, br.y + 4 * z), IM_COL32(80, 150, 220, 255), lbl);
}

// Ambient Air — Ground/atmosphere symbol (horizontal lines)
static void DrawAmbient(ImDrawList* dl, ImVec2 c, float z, bool sel, bool running, const std::vector<double>& temps) {
    float W = 30 * z;
    ImU32 col = sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(140, 200, 140, 255);

    // Three horizontal lines decreasing in width
    dl->AddLine(ImVec2(c.x - W, c.y), ImVec2(c.x + W, c.y), col, 2.0f);
    dl->AddLine(ImVec2(c.x - W * 0.7f, c.y + 6 * z), ImVec2(c.x + W * 0.7f, c.y + 6 * z), col, 2.0f);
    dl->AddLine(ImVec2(c.x - W * 0.35f, c.y + 12 * z), ImVec2(c.x + W * 0.35f, c.y + 12 * z), col, 2.0f);

    // Vertical stem up to port
    dl->AddLine(ImVec2(c.x, c.y - 20 * z), ImVec2(c.x, c.y), col, 1.5f);

    // Temperature label
    if (!temps.empty()) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.0f°C", temps[0]);
        ImVec2 ls = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(c.x - ls.x * 0.5f, c.y - 38 * z), IM_COL32(140, 220, 140, 255), buf);
    }

    const char* lbl = "AMBIENT";
    ImVec2 ls = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2(c.x - ls.x * 0.5f, c.y + 18 * z), IM_COL32(140, 200, 140, 255), lbl);
}

// ─── COMPONENT LIBRARY REGISTRY ───────────────────────────────────────────────

static std::vector<ComponentDef> s_library;
static bool s_library_initialized = false;

static void InitLibrary() {
    if (s_library_initialized) return;
    s_library_initialized = true;

    // ── ENGINE BLOCK ──────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::EngineBlock;
        d.id          = "engine_block";
        d.name        = "Engine Block";
        d.description = "ICE engine block with cylinder head, combustion heat source and coolant jacket";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(80, 40, 10, 255);
        d.borderColor = IM_COL32(220, 140, 50, 255);
        d.width       = 140; d.height = 100;

        d.ports = {
            { "coolant_out", "Coolant Out", PortType::Coolant, +70,    0,  'R', true  },
            { "coolant_in",  "Coolant In",  PortType::Coolant, -70,    0,  'L', false },
            { "heat_out",    "Block Heat",  PortType::Heat,     0,  -50,  'T', true  },
            { "oil_in",      "Oil Gallery", PortType::Oil,      0,  +50,  'B', false }
        };

        d.nodes = {
            // Engine block (solid thermal mass — most heat)
            { 1, "block",  90.0, 55000.0,  15000.0, false, 0, "",      0.0, "block_capacity", "heat_rejection" },
            // Coolant jacket fluid volume
            { 2, "jacket", 80.0, 0.0,       0.0,    false, 1, "Water", 4.5, "",                ""              }
        };
        d.links = {
            // Conduction from block to jacket
            { 1, 1, 2, 0, "block_jacket_cond", "", 1200.0, 0.0 }
        };
        d.portNodeMap = {
            { "coolant_out", 2 }, { "coolant_in", 2 },
            { "heat_out",    1 }, { "oil_in",     1 }
        };
        d.params = {
            { "heat_rejection",    "Heat Rejection",    "W",    "Engine heat rejected to coolant at rated load", 15000.0, 0,     80000, 500  },
            { "block_capacity",    "Block Thermal Mass","J/K",  "Engine block thermal capacity",                 55000.0, 5000,  200000,1000 },
            { "block_jacket_cond", "Block-Jacket cond", "W/K",  "Conductance from block to coolant jacket",       1200.0, 100,   5000,  50   },
            { "jacket_volume",     "Jacket Volume",     "L",    "Coolant jacket fluid volume",                       4.5, 0.5,   15.0,  0.5  }
        };
        d.drawSymbol = DrawEngine;
        s_library.push_back(d);
    }

    // ── RADIATOR ──────────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::Radiator;
        d.id          = "radiator";
        d.name        = "Radiator";
        d.description = "Liquid-to-air heat exchanger with aluminium core and coolant headers";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(15, 30, 60, 255);
        d.borderColor = IM_COL32(60, 130, 220, 255);
        d.width       = 60; d.height = 120;

        d.ports = {
            { "coolant_in",  "Coolant In",  PortType::Coolant,  0, -60, 'T', false },
            { "coolant_out", "Coolant Out", PortType::Coolant,  0, +60, 'B', true  },
            { "air_in",      "Air In",      PortType::Air,    -30,   0, 'L', false },
            { "air_out",     "Air Out",     PortType::Air,    +30,   0, 'R', true  }
        };
        d.nodes = {
            { 1, "coolant", 80.0, 0.0,     0.0, false, 1, "Water", 3.0, "", "" },
            { 2, "core",    55.0, 8000.0,  0.0, false, 0, "",      0.0, "core_capacity", "" }
        };
        d.links = {
            { 1, 1, 2, 1, "coolant_hA", "", 800.0, 0.0 },   // Convection coolant <-> core
            { 2, 2, 2, 1, "air_hA",     "", 500.0, 0.0 }    // Convection core <-> air (node 2 <-> ambient; resolved at connection time)
        };
        d.portNodeMap = {
            { "coolant_in",  1 }, { "coolant_out", 1 },
            { "air_in",      2 }, { "air_out",     2 }
        };
        d.params = {
            { "coolant_hA",    "Coolant-Core hA", "W/K", "Coolant-to-core heat transfer coefficient x area", 800.0,  50, 5000, 50 },
            { "air_hA",        "Core-Air hA",     "W/K", "Core-to-air heat transfer coefficient x area",     500.0,  50, 5000, 50 },
            { "coolant_volume","Coolant Volume",  "L",   "Coolant volume inside radiator",                     3.0, 0.1, 20.0, 0.5 },
            { "core_capacity", "Core Mass",       "J/K", "Radiator core thermal capacity",                  8000.0, 500,50000,500 }
        };
        d.drawSymbol = DrawRadiator;
        s_library.push_back(d);
    }

    // ── RADIATOR FAN ──────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::RadiatorFan;
        d.id          = "radiator_fan";
        d.name        = "Radiator Fan";
        d.description = "Electric radiator fan with P-Q curve — forces air through radiator core";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(25, 12, 40, 255);
        d.borderColor = IM_COL32(160, 90, 220, 255);
        d.width       = 64; d.height = 64;

        d.ports = {
            { "air_in",  "Air In",  PortType::Air, -32, 0, 'L', false },
            { "air_out", "Air Out", PortType::Air, +32, 0, 'R', true  }
        };
        // Fan is a LINK_FAN between upstream and downstream air nodes
        // No internal nodes — it's a link-only component
        d.nodes = {};
        d.links = {};  // connection to ambient and radiator air ports handles this
        d.portNodeMap = {};
        d.params = {
            { "p_max",    "Shut-off Pressure",  "Pa",   "Maximum static pressure at zero flow",          200.0,  10, 2000, 10 },
            { "v_max",    "Free Delivery",       "m/s",  "Max air velocity at zero pressure",              12.0,   1, 50,   0.5 },
            { "K",        "Quadratic Resist.",   "",     "System resistance coefficient K (K*v^2 term)",   0.5,    0, 10,   0.1 },
            { "R",        "Linear Resist.",      "",     "System resistance coefficient R (R*v term)",      2.0,    0, 20,   0.5 },
            { "fan_area", "Fan Area",            "m^2",  "Cross-sectional area of fan",                  0.005, 0.001, 1.0, 0.001 }
        };
        d.drawSymbol = DrawFan;
        s_library.push_back(d);
    }

    // ── WATER PUMP ────────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::WaterPump;
        d.id          = "water_pump";
        d.name        = "Water Pump";
        d.description = "Coolant pump driving fluid around the cooling circuit";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(10, 35, 28, 255);
        d.borderColor = IM_COL32(50, 200, 150, 255);
        d.width       = 48; d.height = 48;

        d.ports = {
            { "coolant_in",  "Inlet",  PortType::Coolant, -24, 0, 'L', false },
            { "coolant_out", "Outlet", PortType::Coolant, +24, 0, 'R', true  }
        };
        d.nodes = {};   // Pure flow link — no thermal mass
        d.links = {};
        d.portNodeMap = {};
        d.params = {
            { "flow_rate", "Flow Rate", "L/min", "Volumetric flow rate of coolant", 20.0, 0.1, 200.0, 1.0 }
        };
        d.drawSymbol = DrawPump;
        s_library.push_back(d);
    }

    // ── THERMOSTAT ────────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::Thermostat;
        d.id          = "thermostat";
        d.name        = "Thermostat";
        d.description = "Wax-element thermostat controlling coolant flow split between bypass and main circuit";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(15, 40, 25, 255);
        d.borderColor = IM_COL32(50, 200, 100, 255);
        d.width       = 56; d.height = 56;

        d.ports = {
            { "inlet",      "Inlet",        PortType::Coolant,  0,  -28, 'T', false },
            { "main_out",   "Main Circuit",  PortType::Coolant, +28,   0, 'R', true  },
            { "bypass_out", "Bypass",        PortType::Coolant, -28,   0, 'L', true  }
        };
        // Thermostat: two parallel LINK_FLOW outputs from single inlet node
        // Temperature-controlled ratio split: open fraction = f(T_inlet - T_open)
        d.nodes = {
            { 1, "valve_body", 80.0, 200.0, 0.0, false, 0, "", 0.0, "", "" }
        };
        d.links = {};
        d.portNodeMap = {
            { "inlet", 1 }, { "main_out", 1 }, { "bypass_out", 1 }
        };
        d.params = {
            { "open_temp",   "Opening Temp",    "°C", "Temperature at which thermostat starts to open",   82.0,  50, 110, 1.0 },
            { "full_open",   "Fully Open Temp", "°C", "Temperature at which thermostat is fully open",    95.0,  60, 120, 1.0 },
            { "main_flow",   "Main Flow Rate",  "L/min", "Full-open main circuit flow rate",              20.0,  1,  100, 1.0 },
            { "bypass_flow", "Bypass Flow Rate","L/min", "Full-bypass flow rate (thermostat closed)",      5.0,  0,   50, 1.0 }
        };
        d.drawSymbol = DrawThermostat;
        s_library.push_back(d);
    }

    // ── COOLANT HOSE ──────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::CoolantHose;
        d.id          = "coolant_hose";
        d.name        = "Coolant Hose";
        d.description = "Rubber coolant hose with fluid thermal mass and small heat loss to ambient";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(20, 40, 70, 255);
        d.borderColor = IM_COL32(60, 120, 200, 255);
        d.width       = 100; d.height = 28;

        d.ports = {
            { "coolant_in",  "Inlet",  PortType::Coolant, -50, 0, 'L', false },
            { "coolant_out", "Outlet", PortType::Coolant, +50, 0, 'R', true  }
        };
        d.nodes = {
            { 1, "fluid", 75.0, 0.0, 0.0, false, 1, "Water", 0.5, "", "" }
        };
        d.links = {};
        d.portNodeMap = {
            { "coolant_in", 1 }, { "coolant_out", 1 }
        };
        d.params = {
            { "hose_volume", "Fluid Volume", "L",   "Volume of coolant inside hose",        0.5, 0.01, 5.0, 0.05 },
            { "heat_loss",   "Heat Loss",    "W/K", "Convection loss to ambient (h*A)",      2.0,  0,   50,  0.5  }
        };
        d.drawSymbol = DrawHose;
        s_library.push_back(d);
    }

    // ── OIL COOLER ────────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::OilCooler;
        d.id          = "oil_cooler";
        d.name        = "Oil Cooler";
        d.description = "Engine oil cooler — oil-to-coolant plate heat exchanger";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(35, 20, 5, 255);
        d.borderColor = IM_COL32(200, 130, 30, 255);
        d.width       = 80; d.height = 80;

        d.ports = {
            { "oil_in",      "Oil In",      PortType::Oil,     -40, -20, 'L', false },
            { "oil_out",     "Oil Out",     PortType::Oil,     -40, +20, 'L', true  },
            { "coolant_in",  "Coolant In",  PortType::Coolant, +40, -20, 'R', false },
            { "coolant_out", "Coolant Out", PortType::Coolant, +40, +20, 'R', true  }
        };
        d.nodes = {
            { 1, "oil",     100.0, 0.0, 0.0, false, 1, "Oil",   0.8, "", "" },
            { 2, "coolant",  80.0, 0.0, 0.0, false, 1, "Water", 0.5, "", "" }
        };
        d.links = {
            { 1, 1, 2, 1, "oil_coolant_hA", "", 400.0, 0.0 }
        };
        d.portNodeMap = {
            { "oil_in",      1 }, { "oil_out",     1 },
            { "coolant_in",  2 }, { "coolant_out", 2 }
        };
        d.params = {
            { "oil_coolant_hA", "Oil-Coolant hA", "W/K", "Heat transfer coefficient x area between oil and coolant", 400.0, 10, 3000, 20 },
            { "oil_volume",     "Oil Volume",      "L",   "Engine oil volume in cooler",                               0.8,  0.05, 10, 0.05 },
            { "coolant_volume", "Coolant Volume",  "L",   "Coolant volume in cooler",                                  0.5,  0.05, 5,  0.05 }
        };
        d.drawSymbol = DrawOilCooler;
        s_library.push_back(d);
    }

    // ── HEATER CORE ──────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::HeaterCore;
        d.id          = "heater_core";
        d.name        = "Heater Core";
        d.description = "Cabin heater core — coolant-to-air heat exchanger for passenger heating";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(40, 10, 10, 255);
        d.borderColor = IM_COL32(220, 80, 80, 255);
        d.width       = 90; d.height = 56;

        d.ports = {
            { "coolant_in",  "Coolant In",  PortType::Coolant, -45, 0, 'L', false },
            { "coolant_out", "Coolant Out", PortType::Coolant, +45, 0, 'R', true  },
            { "air_in",      "Cabin Air In",PortType::Air,       0,-28, 'T', false },
            { "air_out",     "Warm Air Out",PortType::Air,       0,+28, 'B', true  }
        };
        d.nodes = {
            { 1, "coolant", 75.0, 0.0,    0.0, false, 1, "Water", 0.6, "", "" },
            { 2, "core",    60.0, 800.0,  0.0, false, 0, "",      0.0, "core_capacity", "" }
        };
        d.links = {
            { 1, 1, 2, 1, "coolant_hA", "", 300.0, 0.0 }
        };
        d.portNodeMap = {
            { "coolant_in",  1 }, { "coolant_out", 1 },
            { "air_in",      2 }, { "air_out",     2 }
        };
        d.params = {
            { "coolant_hA",    "Coolant-Core hA", "W/K", "Coolant-to-core heat transfer",          300.0,  10, 3000, 10  },
            { "core_capacity", "Core Thermal Mass","J/K","Heater core thermal capacity",             800.0, 100, 5000, 100 },
            { "coolant_volume","Coolant Volume",   "L",  "Coolant volume in heater core",              0.6, 0.05, 5,  0.05 }
        };
        d.drawSymbol = DrawHeaterCore;
        s_library.push_back(d);
    }

    // ── EXPANSION TANK ────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::ExpansionTank;
        d.id          = "expansion_tank";
        d.name        = "Expansion Tank";
        d.description = "Coolant expansion / degas bottle — thermal buffer and system pressure reference";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(15, 35, 60, 255);
        d.borderColor = IM_COL32(60, 130, 210, 255);
        d.width       = 44; d.height = 70;

        d.ports = {
            { "coolant_port", "Coolant", PortType::Coolant, 0, +35, 'B', false }
        };
        d.nodes = {
            { 1, "coolant", 70.0, 0.0, 0.0, false, 1, "Water", 1.5, "", "" }
        };
        d.links = {};
        d.portNodeMap = {
            { "coolant_port", 1 }
        };
        d.params = {
            { "tank_volume", "Tank Volume", "L", "Coolant volume in expansion tank", 1.5, 0.1, 10.0, 0.1 }
        };
        d.drawSymbol = DrawExpansionTank;
        s_library.push_back(d);
    }

    // ── AMBIENT AIR ───────────────────────────────────────────────────────────
    {
        ComponentDef d;
        d.type        = CompType::AmbientAir;
        d.id          = "ambient_air";
        d.name        = "Ambient Air";
        d.description = "Fixed temperature ambient air boundary — infinite thermal reservoir";
        d.category    = "ICE";
        d.bodyColor   = IM_COL32(20, 40, 20, 255);
        d.borderColor = IM_COL32(140, 200, 140, 255);
        d.width       = 60; d.height = 50;

        d.ports = {
            { "air_port", "Air Connection", PortType::Air, 0, -25, 'T', false }
        };
        d.nodes = {
            { 1, "ambient", 25.0, 1.0, 0.0, true,  0, "", 0.0, "", "" }  // is_fixed = true
        };
        d.links = {};
        d.portNodeMap = {
            { "air_port", 1 }
        };
        d.params = {
            { "temp_c", "Temperature", "°C", "Ambient temperature (fixed boundary)", 25.0, -50.0, 60.0, 1.0 }
        };
        d.drawSymbol = DrawAmbient;
        s_library.push_back(d);
    }
}

// ─── PUBLIC API ───────────────────────────────────────────────────────────────

const std::vector<ComponentDef>& GetComponentLibrary() {
    InitLibrary();
    return s_library;
}

const ComponentDef* GetCompDef(CompType type) {
    InitLibrary();
    for (const auto& d : s_library)
        if (d.type == type) return &d;
    return nullptr;
}

const ComponentDef* GetCompDefById(const std::string& id) {
    InitLibrary();
    for (const auto& d : s_library)
        if (d.id == id) return &d;
    return nullptr;
}

// Returns canvas-space port position for a given component instance
ImVec2 GetPortCanvasPos(const CompInstance& inst, const ComponentDef& def, const std::string& portId) {
    for (const auto& p : def.ports) {
        if (p.id == portId) {
            // TODO: apply rotation
            return ImVec2(inst.x + p.dx, inst.y + p.dy);
        }
    }
    return ImVec2(inst.x, inst.y);
}

ImVec2 GetPortWorldPos(const CompInstance& inst, const ComponentDef& def, const std::string& portId,
                       ImVec2 canvasOrigin, float zoom, ImVec2 scrolling) {
    ImVec2 canvas = GetPortCanvasPos(inst, def, portId);
    return ImVec2(
        canvasOrigin.x + (canvas.x + scrolling.x) * zoom,
        canvasOrigin.y + (canvas.y + scrolling.y) * zoom
    );
}

// ─── COMPILE COMPONENT GRAPH → FLAT SOLVER ARRAYS ────────────────────────────

bool CompileComponentGraph(
    const std::vector<CompInstance>&   instances,
    const std::vector<CompConnection>& connections,
    const std::vector<ComponentDef>&   library,
    std::vector<DesktopNode>&          outNodes,
    std::vector<DesktopLink>&          outLinks)
{
    outNodes.clear();
    outLinks.clear();

    int linkIdCounter = 10000;

    // Helper: global solver node ID from (instId, localNodeId)
    auto globalNodeId = [](int instId, int localId) -> int {
        return instId * 100 + localId;
    };

    // Step 1: Add all internal component nodes
    for (const auto& inst : instances) {
        const ComponentDef* def = GetCompDefById(inst.defId);
        if (!def) continue;

        for (const auto& nd : def->nodes) {
            DesktopNode dn;
            dn.id       = globalNodeId(inst.instId, nd.localId);
            dn.name     = inst.defId + "_" + nd.role + "_" + std::to_string(inst.instId);
            dn.x        = inst.x;
            dn.y        = inst.y;
            dn.temp     = nd.tempC;
            dn.is_fixed = nd.is_fixed;
            dn.domain   = nd.domain;

            // Resolve capacity from param
            if (!nd.capacity_param.empty()) {
                auto it = inst.params.find(nd.capacity_param);
                dn.capacity = (it != inst.params.end()) ? it->second : nd.capacity;
            } else {
                dn.capacity = nd.capacity;
            }

            // Resolve q_gen from param
            if (!nd.q_gen_param.empty()) {
                auto it = inst.params.find(nd.q_gen_param);
                dn.q_gen = (it != inst.params.end()) ? it->second : nd.q_gen;
            } else {
                dn.q_gen = nd.q_gen;
            }

            // Fluid properties
            dn.fluid_medium = nd.fluid_medium;

            // Fluid volume from param (look for *_volume param)
            if (nd.domain == 1) {
                std::string volKey = nd.role + "_volume";
                auto it = inst.params.find(volKey);
                if (it == inst.params.end()) it = inst.params.find("hose_volume");
                if (it == inst.params.end()) it = inst.params.find("tank_volume");
                dn.fluid_volume = (it != inst.params.end()) ? it->second : nd.fluid_volume;
            }

            // Special: AmbientAir temperature from "temp_c" param
            if (nd.is_fixed) {
                auto it = inst.params.find("temp_c");
                if (it != inst.params.end()) dn.temp = it->second;
            }

            outNodes.push_back(dn);
        }

        // Step 2: Add internal component links
        for (const auto& ld : def->links) {
            DesktopLink dl;
            dl.id     = linkIdCounter++;
            dl.node_a = globalNodeId(inst.instId, ld.nodeA);
            dl.node_b = globalNodeId(inst.instId, ld.nodeB);
            dl.type   = ld.type;

            // Resolve p1 from param
            auto it = inst.params.find(ld.p1_param);
            dl.p1 = (it != inst.params.end()) ? it->second : ld.p1_default;

            // Resolve p2
            auto it2 = inst.params.find(ld.p2_param);
            dl.p2 = (it2 != inst.params.end()) ? it2->second : ld.p2_default;

            dl.fan_area = 0.005;
            outLinks.push_back(dl);
        }
    }

    // Step 3: Add connections between components as LINK_FLOW links
    for (const auto& conn : connections) {
        // Find from-component def and which local node "fromPort" maps to
        const CompInstance* fromInst = nullptr;
        const CompInstance* toInst   = nullptr;
        for (const auto& inst : instances) {
            if (inst.instId == conn.fromInstId) fromInst = &inst;
            if (inst.instId == conn.toInstId)   toInst   = &inst;
        }
        if (!fromInst || !toInst) continue;

        const ComponentDef* fromDef = GetCompDefById(fromInst->defId);
        const ComponentDef* toDef   = GetCompDefById(toInst->defId);
        if (!fromDef || !toDef) continue;

        // Find local node IDs for the ports
        int fromLocalNode = -1, toLocalNode = -1;
        for (const auto& pm : fromDef->portNodeMap) {
            if (pm.portId == conn.fromPortId) { fromLocalNode = pm.localNodeId; break; }
        }
        for (const auto& pm : toDef->portNodeMap) {
            if (pm.portId == conn.toPortId) { toLocalNode = pm.localNodeId; break; }
        }

        // Fan components (RadiatorFan, WaterPump) have no internal nodes —
        // they become a LINK_FAN or LINK_FLOW directly on the connection
        bool fromIsFanLike = fromDef->nodes.empty();
        bool toIsFanLike   = toDef->nodes.empty();

        if (fromIsFanLike && toIsFanLike) continue;  // degenerate

        int nodeA = -1, nodeB = -1;

        if (!fromIsFanLike && fromLocalNode > 0)
            nodeA = globalNodeId(conn.fromInstId, fromLocalNode);
        if (!toIsFanLike && toLocalNode > 0)
            nodeB = globalNodeId(conn.toInstId, toLocalNode);

        // Skip if we can't resolve both ends
        if (nodeA < 0 || nodeB < 0) continue;

        // Determine link type
        int linkType = 3; // LINK_FLOW default for coolant/oil
        double p1 = conn.flow_rate;
        double p2 = 1.0;
        double fanArea = 0.005;

        if (conn.portType == PortType::Air) {
            linkType = 1; // Convection for air connections
            p1 = 500.0;   // Default air-side hA
            p2 = 0.0;
        }

        // Special: if one side is a RadiatorFan — use LINK_FAN
        auto isFanComp = [](const ComponentDef* def) {
            return def->type == CompType::RadiatorFan;
        };
        if (isFanComp(fromDef) || isFanComp(toDef)) {
            linkType = 4; // LINK_FAN
            const CompInstance* fanInst = isFanComp(fromDef) ? fromInst : toInst;
            auto it = fanInst->params.find("p_max");
            p1 = (it != fanInst->params.end()) ? it->second : 200.0;
            it = fanInst->params.find("v_max");
            p2 = (it != fanInst->params.end()) ? it->second : 12.0;
            it = fanInst->params.find("fan_area");
            fanArea = (it != fanInst->params.end()) ? it->second : 0.005;
        }

        // Special: if one side is a WaterPump — use LINK_FLOW with pump's flow rate
        auto isPumpComp = [](const ComponentDef* def) {
            return def->type == CompType::WaterPump;
        };
        if (isPumpComp(fromDef) || isPumpComp(toDef)) {
            linkType = 3; // LINK_FLOW
            const CompInstance* pumpInst = isPumpComp(fromDef) ? fromInst : toInst;
            auto it = pumpInst->params.find("flow_rate");
            p1 = (it != pumpInst->params.end()) ? it->second : 20.0;
        }

        DesktopLink dl;
        dl.id       = linkIdCounter++;
        dl.node_a   = nodeA;
        dl.node_b   = nodeB;
        dl.type     = linkType;
        dl.p1       = p1;
        dl.p2       = p2;
        dl.fan_area = fanArea;
        dl.g_a1 = dl.g_a2 = 0.0;
        outLinks.push_back(dl);
    }

    return true;
}
