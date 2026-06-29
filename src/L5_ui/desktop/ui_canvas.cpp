#include "ui_canvas.h"
#include "../../L3_state/app_state.h"
#include <cmath>
#include <algorithm>

std::vector<PortPos> GetPortPositions(std::shared_ptr<DesktopNode> node) {
    std::vector<PortPos> p;
    for (const auto& pd : node->GetPorts()) {
        p.push_back({ pd.face, ImVec2(node->x + pd.dx, node->y + pd.dy) });
    }
    // Fallback if no ports defined: return R,L,T,B offsets
    if (p.empty()) {
        float w, h;
        node->GetBounds(w, h);
        p.push_back({ 'R', ImVec2(node->x + w / 2.0f, node->y) });
        p.push_back({ 'L', ImVec2(node->x - w / 2.0f, node->y) });
        p.push_back({ 'T', ImVec2(node->x, node->y - h / 2.0f) });
        p.push_back({ 'B', ImVec2(node->x, node->y + h / 2.0f) });
    }
    return p;
}

PortPos GetClosestPort(std::shared_ptr<DesktopNode> nodeThis, std::shared_ptr<DesktopNode> nodeOther) {
    auto ports = GetPortPositions(nodeThis);
    float minDist = 1e9f;
    PortPos closest = ports[0];
    for (const auto& p : ports) {
        float d = std::pow(p.pos.x - nodeOther->x, 2.0f) + std::pow(p.pos.y - nodeOther->y, 2.0f);
        if (d < minDist) {
            minDist = d;
            closest = p;
        }
    }
    return closest;
}

std::vector<ImVec2> ComputeOrthogonalRoute(ImVec2 portA, char faceA, ImVec2 portB, char faceB) {
    std::vector<ImVec2> route;
    route.push_back(portA);

    float midX = 0.5f * (portA.x + portB.x);
    float midY = 0.5f * (portA.y + portB.y);

    if (faceA == 'R' || faceA == 'L') {
        route.push_back(ImVec2(midX, portA.y));
        route.push_back(ImVec2(midX, portB.y));
    } else {
        route.push_back(ImVec2(portA.x, midY));
        route.push_back(ImVec2(portB.x, midY));
    }

    route.push_back(portB);
    return route;
}

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

void DrawSchematicCanvas(ImDrawList* draw_list, ImVec2 canvas_pos, ImVec2 canvas_size) {
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
    ImGui::TextDisabled(" | Zoom: %.0f%%  |  %s mode", g_canvas_zoom * 100.0f, "Simulation");
    
    ImGui::Separator();
    
    // Draw background and borders
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
        if (!g_comp_mode && ImGui::IsMouseDoubleClicked(0)) {
            auto hit_node = getNodeAt(local_mouse);
            if (!hit_node) {
                float x = local_mouse.x;
                float y = local_mouse.y;
                if (g_grid_snap) {
                    x = std::round(x / GRID_SIZE) * GRID_SIZE;
                    y = std::round(y / GRID_SIZE) * GRID_SIZE;
                }
                int nextId = 1;
                if (!g_nodes.empty()) {
                    auto max_it = std::max_element(g_nodes.begin(), g_nodes.end(), [](const std::shared_ptr<DesktopNode>& a, const std::shared_ptr<DesktopNode>& b){
                        return a->id < b->id;
                    });
                    nextId = (*max_it)->id + 1;
                }
                PushUndoState();
                auto newNode = std::make_shared<DesktopNode>();
                newNode->id = nextId;
                newNode->name = "Mass " + std::to_string(nextId);
                newNode->x = x;
                newNode->y = y;
                newNode->temp = 25.0;
                newNode->capacity = 500.0;
                newNode->q_gen = 0.0;
                newNode->is_fixed = false;
                newNode->temp_init = 25.0;
                g_nodes.push_back(newNode);

                g_plot_active_nodes[nextId] = true;
                SyncSystemWithSolver();
                ResetHistory();
                g_selected_node = g_nodes.back();
                g_selected_link = nullptr;
                Log("Placed node component [Mass " + std::to_string(nextId) + "] on canvas.", "info");
            }
        } else if (ImGui::IsMouseClicked(0)) {
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
