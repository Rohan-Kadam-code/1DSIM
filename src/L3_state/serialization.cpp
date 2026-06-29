#include "serialization.h"
#include "app_state.h"
#include "../../thirdparty/json.hpp"
#include <windows.h>
#include <commdlg.h>
#include <fstream>

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
                        engine->jacketNode->id = jn["id"].get<int>() + 1;
                        engine->internalCond->id = jn["id"].get<int>() + 2;
                        engine->internalCond->node_a = jn["id"];
                        engine->internalCond->node_b = engine->jacketNode->id;
                        n = engine;
                    } else if (nodeName == "Radiator") {
                        auto radiator = std::make_shared<RadiatorNode>();
                        radiator->coreNode->id = jn["id"].get<int>() + 1;
                        radiator->internalConv->id = jn["id"].get<int>() + 2;
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
