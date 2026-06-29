#include "presets.h"
#include "app_state.h"
#include "../L2_domain/sim_elements.h"

void UpdateThermostatBypassFlows() {
    if (g_active_preset != "complete_loop") return;
    
    // Find the Thermostat Node (ID 500)
    std::shared_ptr<DesktopNode> tstat = nullptr;
    for (const auto& n : g_nodes) {
        if (n->id == 500) {
            tstat = n;
            break;
        }
    }
    
    // Find the Water Pump Node speed (ID 400)
    double speed_frac = 1.0;
    for (const auto& n : g_nodes) {
        if (n->id == 400) {
            auto it = n->params.find("speed");
            if (it != n->params.end()) {
                speed_frac = it->second;
            }
            break;
        }
    }
    
    // Find the flow link from pump to Hose 5 (ID 1009) to get the base pump rate slider setting
    double Q_pump_base = 35.0;
    std::shared_ptr<DesktopLink> link_pump = nullptr;
    for (const auto& l : g_links) {
        if (l->id == 1009) {
            link_pump = l;
            Q_pump_base = l->p1;
            break;
        }
    }
    
    double Q_pump = Q_pump_base * speed_frac;
    
    // Calculate thermostat open fraction based on its current temperature
    double t_c = tstat ? tstat->temp : 25.0;
    double open_temp = 82.0;
    double full_open = 95.0;
    if (tstat) {
        auto it = tstat->params.find("open_temp");
        if (it != tstat->params.end()) open_temp = it->second;
        it = tstat->params.find("full_open");
        if (it != tstat->params.end()) full_open = it->second;
    }
    
    double open_frac = 0.0;
    if (t_c > open_temp) {
        if (t_c >= full_open) open_frac = 1.0;
        else open_frac = (t_c - open_temp) / (full_open - open_temp);
    }
    
    double Q_main = Q_pump * open_frac;
    double Q_bypass = Q_pump * (1.0 - open_frac);
    
    // Update the flow rates of all loop links
    for (auto& l : g_links) {
        if (l->id == 1001 || l->id == 1002 || l->id == 1009 || l->id == 1010) {
            l->p1 = Q_pump;
        } else if (l->id == 1003 || l->id == 1004 || l->id == 1005 || l->id == 1006) {
            l->p1 = Q_main;
        } else if (l->id == 1007 || l->id == 1008) {
            l->p1 = Q_bypass;
        }
    }
}

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
    else if (key == "complete_loop") {
        // 1. Engine Block
        auto engine = std::make_shared<EngineBlockNode>();
        engine->id = 100;
        engine->x = 160.0f;
        engine->y = 280.0f;
        engine->jacketNode->id = 101;
        engine->internalCond->id = 102;
        engine->internalCond->node_a = 100;
        engine->internalCond->node_b = 101;
        engine->capacity = engine->params["block_capacity"];
        engine->q_gen = engine->params["heat_rejection"];
        engine->jacketNode->fluid_volume = engine->params["jacket_volume"];
        engine->internalCond->p1 = engine->params["block_jacket_cond"];
        g_nodes.push_back(engine);

        // 2. Hose 1 (Engine to Tstat)
        auto hose1 = std::make_shared<CoolantHoseNode>();
        hose1->id = 600;
        hose1->name = "Hose: Engine to Tstat";
        hose1->x = 310.0f;
        hose1->y = 200.0f;
        hose1->temp = 25.0;
        hose1->fluid_volume = 0.5;
        g_nodes.push_back(hose1);

        // 3. Thermostat
        auto tstat = std::make_shared<ThermostatNode>();
        tstat->id = 500;
        tstat->x = 460.0f;
        tstat->y = 200.0f;
        tstat->temp = 25.0;
        tstat->fluid_volume = 0.2;
        g_nodes.push_back(tstat);

        // 4. Hose 2 (Tstat to Radiator)
        auto hose2 = std::make_shared<CoolantHoseNode>();
        hose2->id = 610;
        hose2->name = "Hose: Tstat to Radiator";
        hose2->x = 610.0f;
        hose2->y = 200.0f;
        hose2->temp = 25.0;
        hose2->fluid_volume = 0.5;
        g_nodes.push_back(hose2);

        // 5. Radiator
        auto radiator = std::make_shared<RadiatorNode>();
        radiator->id = 200;
        radiator->x = 760.0f;
        radiator->y = 280.0f;
        radiator->coreNode->id = 201;
        radiator->internalConv->id = 202;
        radiator->internalConv->node_a = 200;
        radiator->internalConv->node_b = 201;
        radiator->fluid_volume = radiator->params["coolant_volume"];
        radiator->coreNode->capacity = radiator->params["core_capacity"];
        radiator->internalConv->p1 = radiator->params["coolant_hA"];
        g_nodes.push_back(radiator);

        // 6. Hose 3 (Radiator to Pump)
        auto hose3 = std::make_shared<CoolantHoseNode>();
        hose3->id = 620;
        hose3->name = "Hose: Radiator to Pump";
        hose3->x = 610.0f;
        hose3->y = 420.0f;
        hose3->temp = 25.0;
        hose3->fluid_volume = 0.5;
        g_nodes.push_back(hose3);

        // 7. Water Pump
        auto pump = std::make_shared<WaterPumpNode>();
        pump->id = 400;
        pump->x = 460.0f;
        pump->y = 420.0f;
        pump->temp = 25.0;
        pump->fluid_volume = 0.5;
        g_nodes.push_back(pump);

        // 8. Hose 4 (Bypass to Pump)
        auto hose4 = std::make_shared<CoolantHoseNode>();
        hose4->id = 630;
        hose4->name = "Hose: Bypass to Pump";
        hose4->x = 310.0f;
        hose4->y = 320.0f;
        hose4->temp = 25.0;
        hose4->fluid_volume = 0.5;
        g_nodes.push_back(hose4);

        // 9. Hose 5 (Pump to Engine)
        auto hose5 = std::make_shared<CoolantHoseNode>();
        hose5->id = 640;
        hose5->name = "Hose: Pump to Engine";
        hose5->x = 200.0f;
        hose5->y = 420.0f;
        hose5->temp = 25.0;
        hose5->fluid_volume = 0.5;
        g_nodes.push_back(hose5);

        // 10. Expansion Tank
        auto tank = std::make_shared<ExpansionTankNode>();
        tank->id = 700;
        tank->x = 460.0f;
        tank->y = 80.0f;
        tank->temp = 25.0;
        tank->fluid_volume = 1.5;
        g_nodes.push_back(tank);

        // 11. Ambient Air
        auto ambient = std::make_shared<AmbientAirNode>();
        ambient->id = 300;
        ambient->x = 900.0f;
        ambient->y = 280.0f;
        ambient->temp = ambient->params["temp_c"];
        g_nodes.push_back(ambient);

        // --- Links ---
        // Engine -> Hose 1
        auto l1 = std::make_shared<DesktopLink>();
        l1->id = 1001; l1->node_a = 100; l1->node_b = 600; l1->type = 3; l1->p1 = 35.0; l1->p2 = 1.0;
        g_links.push_back(l1);

        // Hose 1 -> Thermostat
        auto l2 = std::make_shared<DesktopLink>();
        l2->id = 1002; l2->node_a = 600; l2->node_b = 500; l2->type = 3; l2->p1 = 35.0; l2->p2 = 1.0;
        g_links.push_back(l2);

        // Thermostat -> Hose 2 (Main)
        auto l3 = std::make_shared<DesktopLink>();
        l3->id = 1003; l3->node_a = 500; l3->node_b = 610; l3->type = 3; l3->p1 = 0.0; l3->p2 = 1.0;
        g_links.push_back(l3);

        // Hose 2 -> Radiator
        auto l4 = std::make_shared<DesktopLink>();
        l4->id = 1004; l4->node_a = 610; l4->node_b = 200; l4->type = 3; l4->p1 = 0.0; l4->p2 = 1.0;
        g_links.push_back(l4);

        // Radiator -> Hose 3
        auto l5 = std::make_shared<DesktopLink>();
        l5->id = 1005; l5->node_a = 200; l5->node_b = 620; l5->type = 3; l5->p1 = 0.0; l5->p2 = 1.0;
        g_links.push_back(l5);

        // Hose 3 -> Pump
        auto l6 = std::make_shared<DesktopLink>();
        l6->id = 1006; l6->node_a = 620; l6->node_b = 400; l6->type = 3; l6->p1 = 0.0; l6->p2 = 1.0;
        g_links.push_back(l6);

        // Thermostat -> Hose 4 (Bypass)
        auto l7 = std::make_shared<DesktopLink>();
        l7->id = 1007; l7->node_a = 500; l7->node_b = 630; l7->type = 3; l7->p1 = 35.0; l7->p2 = 1.0;
        g_links.push_back(l7);

        // Hose 4 -> Pump
        auto l8 = std::make_shared<DesktopLink>();
        l8->id = 1008; l8->node_a = 630; l8->node_b = 400; l8->type = 3; l8->p1 = 35.0; l8->p2 = 1.0;
        g_links.push_back(l8);

        // Pump -> Hose 5
        auto l9 = std::make_shared<DesktopLink>();
        l9->id = 1009; l9->node_a = 400; l9->node_b = 640; l9->type = 3; l9->p1 = 35.0; l9->p2 = 1.0;
        g_links.push_back(l9);

        // Hose 5 -> Engine
        auto l10 = std::make_shared<DesktopLink>();
        l10->id = 1010; l10->node_a = 640; l10->node_b = 100; l10->type = 3; l10->p1 = 35.0; l10->p2 = 1.0;
        g_links.push_back(l10);

        // Radiator Core -> Ambient Air Convection
        auto l11 = std::make_shared<DesktopLink>();
        l11->id = 1011; l11->node_a = 200; l11->node_b = 300; l11->type = 1; l11->p1 = 600.0; l11->p2 = 0.0;
        g_links.push_back(l11);

        // Expansion Tank -> Hose 3 Conduction
        auto l12 = std::make_shared<DesktopLink>();
        l12->id = 1012; l12->node_a = 700; l12->node_b = 620; l12->type = 0; l12->p1 = 0.5; l12->p2 = 0.0;
        g_links.push_back(l12);

        // --- SLIDERS ---
        g_sliders = {
            { "engine_power", "Engine Heat Output (kW)", 0.0f, 120.0f, 25.0f, 2.0f, "node_kw", 100, {}, "q_gen" },
            { "pump_rate", "Water Pump Flow (L/min)", 0.0f, 100.0f, 35.0f, 1.0f, "link", 1009, {}, "p1" },
            { "radiator_fan", "Radiator Cooling Fan (W/K)", 50.0f, 3000.0f, 600.0f, 50.0f, "link", 1011, {}, "p1" },
            { "ambient_temp", "Ambient Environment Temp (C)", 5.0f, 48.0f, 25.0f, 1.0f, "node", 300, {}, "temp" }
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
