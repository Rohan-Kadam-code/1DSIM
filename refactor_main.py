import re

def refactor():
    with open('src/desktop/main.cpp', 'r') as f:
        content = f.read()

    # 1. Include the new sim_elements.h
    content = content.replace('#include "component_library.h"', '#include "sim_elements.h"')

    # 2. Remove old DesktopNode and DesktopLink struct definitions
    start_idx = content.find('struct DesktopNode {')
    end_idx = content.find('};', content.find('struct DesktopLink {')) + 2
    if start_idx != -1 and end_idx != -1:
        content = content[:start_idx] + '// DesktopNode and DesktopLink moved to sim_elements.h\n' + content[end_idx:]

    # 3. Change vector definitions
    content = re.sub(r'std::vector<DesktopNode>\s+g_nodes;', 'std::vector<std::shared_ptr<DesktopNode>> g_nodes;', content)
    content = re.sub(r'std::vector<DesktopLink>\s+g_links;', 'std::vector<std::shared_ptr<DesktopLink>> g_links;', content)
    
    # 4. Change ModelState
    content = re.sub(r'std::vector<DesktopNode>\s+nodes;', 'std::vector<std::shared_ptr<DesktopNode>> nodes;', content)
    content = re.sub(r'std::vector<DesktopLink>\s+links;', 'std::vector<std::shared_ptr<DesktopLink>> links;', content)
    
    model_state_clone = '''
    ModelState Clone() const {
        ModelState copy;
        for (auto& n : nodes) copy.nodes.push_back(n->clone());
        for (auto& l : links) copy.links.push_back(l->clone());
        return copy;
    }
'''
    content = content.replace('std::vector<std::shared_ptr<DesktopLink>> links;\n};', 'std::vector<std::shared_ptr<DesktopLink>> links;\n' + model_state_clone + '};')

    # 5. Fix Undo/Redo pushes
    content = content.replace('g_undo_stack.push_back({ g_nodes, g_links });', 'g_undo_stack.push_back(ModelState{ g_nodes, g_links }.Clone());')
    content = content.replace('g_redo_stack.push_back({ g_nodes, g_links });', 'g_redo_stack.push_back(ModelState{ g_nodes, g_links }.Clone());')
    content = content.replace('g_drag_backup = { g_nodes, g_links };', 'g_drag_backup = ModelState{ g_nodes, g_links }.Clone();')
    
    # 6. Change all pointer access using ->
    props = [
        'id', 'name', 'x', 'y', 'temp', 'capacity', 'q_gen', 'is_fixed', 'temp_init', 
        'c_a1', 'c_a2', 'material', 'mass', 'domain', 'fluid_medium', 'fluid_volume', 
        'fluid_mix_ratio', 'fluid_rho_a0', 'fluid_rho_a1', 'fluid_rho_a2', 
        'fluid_cp_a0', 'fluid_cp_a1', 'fluid_cp_a2',
        'node_a', 'node_b', 'type', 'p1', 'p2', 'g_a1', 'g_a2', 'fan_area'
    ]
    
    for var in ['n', 'l', 'nodeThis', 'nodeOther', 'dn', 'dl', 'targetNode', 'sourceNode', 'nodeA', 'nodeB', 'node_a', 'node_b', 'selNode', 'selLink', 'node', 'link']:
        for prop in props:
            content = re.sub(rf'\b{var}\.{prop}\b', rf'{var}->{prop}', content)

    # 7. Iterators
    for prop in props:
        content = re.sub(rf'\b\(\*itA\)\.{prop}\b', rf'(*itA)->{prop}', content)
        content = re.sub(rf'\b\(\*itB\)\.{prop}\b', rf'(*itB)->{prop}', content)
        content = re.sub(rf'\bitA->{prop}\b', rf'(*itA)->{prop}', content)
        content = re.sub(rf'\bitB->{prop}\b', rf'(*itB)->{prop}', content)
        content = re.sub(rf'\bit->{prop}\b', rf'(*it)->{prop}', content)

    # 8. Fix function parameters and raw pointers
    content = re.sub(r'const DesktopNode&\s+n\b', 'const std::shared_ptr<DesktopNode>& n', content)
    content = re.sub(r'const DesktopLink&\s+l\b', 'const std::shared_ptr<DesktopLink>& l', content)
    content = re.sub(r'const DesktopNode&\s+a\b', 'const std::shared_ptr<DesktopNode>& a', content)
    content = re.sub(r'const DesktopNode&\s+b\b', 'const std::shared_ptr<DesktopNode>& b', content)
    content = re.sub(r'DesktopNode\*\s+dragNode', 'std::shared_ptr<DesktopNode> dragNode', content)
    content = re.sub(r'DesktopNode\*\s+g_selected_node', 'std::shared_ptr<DesktopNode> g_selected_node', content)
    content = re.sub(r'DesktopLink\*\s+g_selected_link', 'std::shared_ptr<DesktopLink> g_selected_link', content)
    content = re.sub(r'DesktopNode\*\s+g_linking_start_node', 'std::shared_ptr<DesktopNode> g_linking_start_node', content)
    content = re.sub(r'DesktopNode\*\s+node\b', 'std::shared_ptr<DesktopNode> node', content)
    content = re.sub(r'DesktopLink\*\s+link\b', 'std::shared_ptr<DesktopLink> link', content)
    content = re.sub(r'DesktopNode\*\s+a\b', 'std::shared_ptr<DesktopNode> a', content)
    content = re.sub(r'DesktopNode\*\s+b\b', 'std::shared_ptr<DesktopNode> b', content)
    
    content = re.sub(r'DesktopNode\s+n;', 'auto n = std::make_shared<DesktopNode>();', content)
    content = re.sub(r'DesktopLink\s+l;', 'auto l = std::make_shared<DesktopLink>();', content)
    content = re.sub(r'DesktopNode\s+dn;', 'auto dn = std::make_shared<DesktopNode>();', content)
    content = re.sub(r'DesktopLink\s+dl;', 'auto dl = std::make_shared<DesktopLink>();', content)
    
    # 9. Pointer assignment fixes
    content = content.replace('g_selected_node = &n;', 'g_selected_node = n;')
    content = content.replace('g_selected_link = &l;', 'g_selected_link = l;')
    content = content.replace('dragNode = &n;', 'dragNode = n;')
    content = content.replace('g_linking_start_node = &n;', 'g_linking_start_node = n;')
    content = content.replace('g_selected_node == &n', 'g_selected_node == n')
    content = content.replace('g_selected_link == &l', 'g_selected_link == l')

    # Remove the whole g_comp_instances related panels and logic!
    panel1_start = content.find('// --- PANEL 1: OBJECT LIBRARY ---')
    panel1_end = content.find('// --- PANEL 2: PROPERTIES ---')
    if panel1_start != -1 and panel1_end != -1:
        new_panel1 = '''// --- PANEL 1: OBJECT LIBRARY ---
    ImGui::Begin("Object Library", nullptr, ImGuiWindowFlags_NoCollapse);
    if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Engine Block", ImVec2(-1, 30))) {
            PlaceComponent("engine_block", 0, 0);
        }
        if (ImGui::Button("Radiator", ImVec2(-1, 30))) {
            PlaceComponent("radiator", 0, 0);
        }
        if (ImGui::Button("Ambient Air", ImVec2(-1, 30))) {
            PlaceComponent("ambient_air", 0, 0);
        }
    }
    if (ImGui::CollapsingHeader("Nodes & Links", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "lib_cols", false);
        ImGui::Button("[Node] Mass\\n(Node)", ImVec2(90.0f, 40.0f));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Double click on empty canvas to place a Thermal Mass node.");
        ImGui::NextColumn();
        ImGui::Button("[Fixed] Boundary\\n(Fixed T)", ImVec2(90.0f, 40.0f));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Double click to create, then toggle 'Fixed Temperature' in Attribute sheet.");
        ImGui::NextColumn();
        ImGui::Button("Conduction\\n(Link)", ImVec2(90.0f, 30.0f));
        ImGui::NextColumn();
        ImGui::Button("Convection\\n(Link)", ImVec2(90.0f, 30.0f));
        ImGui::NextColumn();
        ImGui::Button("Radiation\\n(Link)", ImVec2(90.0f, 30.0f));
        ImGui::NextColumn();
        ImGui::Button("Flow Loop\\n(Advection)", ImVec2(90.0f, 30.0f));
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
    ImGui::End();
    
'''
        content = content[:panel1_start] + new_panel1 + content[panel1_end:]

    # Remove PANEL 2 g_comp_instances property UI entirely
    content = re.sub(r'if \(g_comp_mode\) \{[\s\S]*?\} else \{', 'if (false) { } else {', content)

    # Remove Comp Library rendering entirely
    comp_lib_layer = content.find('// ─── COMPONENT LIBRARY LAYER ────────────────────────────────────────────────')
    canvas_interactions = content.find('// --- CANVAS INTERACTIONS (CLICKS, DRAGS, NEW NODES) ---')
    if comp_lib_layer != -1 and canvas_interactions != -1:
        content = content[:comp_lib_layer] + content[canvas_interactions:]

    # Fix functions
    resolve_pointers = """
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
"""
    content = re.sub(r'static void ResolvePointers\(\) \{[\s\S]*?\}\n\n// Synchronise selection', resolve_pointers + '\n// Synchronise selection', content)

    sync_selection = """
static void SyncSelection() {
    ResolvePointers();
}
"""
    content = re.sub(r'static void SyncSelection\(\) \{[\s\S]*?\}\n\n// Compile component graph', sync_selection + '\n// Compile component graph', content)

    sync_components = """
static void SyncComponentsWithSolver() {
    ResolvePointers();
    SyncSystemWithSolver();
    ResetHistory();
}
"""
    content = re.sub(r'static void SyncComponentsWithSolver\(\) \{[\s\S]*?\}\n\n// Place a component instance', sync_components + '\n// Place a component instance', content)

    place_component = """
static void PlaceComponent(const std::string& defId, float cx, float cy) {
    PushUndoState();
    if (defId == "engine_block") {
        auto node = std::make_shared<EngineBlockNode>();
        node->id = g_nodes.empty() ? 100 : (*std::max_element(g_nodes.begin(), g_nodes.end(), [](auto& a, auto& b){ return a->id < b->id; }))->id + 100;
        node->x = cx; node->y = cy;
        g_nodes.push_back(node);
    } else if (defId == "radiator") {
        auto node = std::make_shared<RadiatorNode>();
        node->id = g_nodes.empty() ? 100 : (*std::max_element(g_nodes.begin(), g_nodes.end(), [](auto& a, auto& b){ return a->id < b->id; }))->id + 100;
        node->x = cx; node->y = cy;
        g_nodes.push_back(node);
    } else if (defId == "ambient_air") {
        auto node = std::make_shared<AmbientAirNode>();
        node->id = g_nodes.empty() ? 100 : (*std::max_element(g_nodes.begin(), g_nodes.end(), [](auto& a, auto& b){ return a->id < b->id; }))->id + 100;
        node->x = cx; node->y = cy;
        g_nodes.push_back(node);
    }
    SyncComponentsWithSolver();
}
"""
    content = re.sub(r'static void PlaceComponent\(const std::string& defId, float cx, float cy\) \{[\s\S]*?\}\n\n// Draw all component instances', place_component + '\n// Draw all component instances', content)

    content = re.sub(r'static void DrawComponentInstances\(.*?\) \{[\s\S]*?\}\n\n// Draw component connections', 'static void DrawComponentInstances(ImDrawList* dl, ImVec2 canvas_origin, float zoom) {}\n\n// Draw component connections', content)
    content = re.sub(r'static void DrawComponentConnections\(.*?\) \{[\s\S]*?\}\n\n// Canvas coordinate helper functions', 'static void DrawComponentConnections(ImDrawList* dl, ImVec2 canvas_origin, float zoom) {}\n\n// Canvas coordinate helper functions', content)

    content = re.sub(r'static const ComponentDef\* GetInstDef\(const CompInstance& inst\) \{[\s\S]*?\}\n', '', content)
    content = re.sub(r'static std::vector<CompInstance>.*?;\n', '', content)
    content = re.sub(r'static std::vector<CompConnection>.*?;\n', '', content)
    content = re.sub(r'static CompInstance\*.*?= nullptr;\n', '', content)
    content = re.sub(r'static CompConnection\*.*?= nullptr;\n', '', content)
    
    content = content.replace('DrawComponentConnections(dl, canvas_origin, zoom);', '')
    content = content.replace('DrawComponentInstances(dl, canvas_origin, zoom);', '')
    
    content = content.replace('void DrawNode(const DesktopNode& n,', 'void DrawNode(const std::shared_ptr<DesktopNode>& n,')
    content = content.replace('void DrawLink(const DesktopLink& l,', 'void DrawLink(const std::shared_ptr<DesktopLink>& l,')
    content = content.replace('n.DrawSymbol(', 'n->DrawSymbol(')
    
    content = content.replace('if (!g_comp_mode) {', 'if (true) {')
    content = content.replace('g_comp_mode ? "Component Library" : "Generic Nodes"', '"Simulation"')
    content = content.replace('static bool          g_comp_mode = true;', 'static bool g_comp_mode = false;')

    # Fix DesktopNode tempTargetNode = { ... };
    content = content.replace('DesktopNode tempTargetNode = { 0, "", local_mouse.x, local_mouse.y, 0.0, 0.0, 0.0, false };', 
                              'auto tempTargetNode = std::make_shared<DesktopNode>(); tempTargetNode->x = local_mouse.x; tempTargetNode->y = local_mouse.y;')
    content = content.replace('PortPos portA = GetClosestPort(*g_linking_start_node, tempTargetNode);',
                              'PortPos portA = GetClosestPort(g_linking_start_node, tempTargetNode);')
    content = content.replace('PortPos GetClosestPort(const std::shared_ptr<DesktopNode>& a, const std::shared_ptr<DesktopNode>& b)',
                              'PortPos GetClosestPort(std::shared_ptr<DesktopNode> a, std::shared_ptr<DesktopNode> b)')

    with open('src/desktop/main.cpp', 'w') as f:
        f.write(content)

if __name__ == "__main__":
    refactor()
