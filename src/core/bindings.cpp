#include "solver.h"

#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C"
#endif

EXPORT void* create_system() {
    return new ThermalSystem();
}

EXPORT void destroy_system(void* system) {
    delete static_cast<ThermalSystem*>(system);
}

EXPORT void add_node(void* system, int id, const char* name, double init_temp, double capacity, double q_gen, bool is_fixed, double c_a1, double c_a2, int domain_type, const char* fluid_medium, double fluid_volume) {
    static_cast<ThermalSystem*>(system)->add_node(id, name, init_temp, capacity, q_gen, is_fixed, c_a1, c_a2, domain_type, fluid_medium, fluid_volume);
}

EXPORT void update_node(void* system, int id, double temp, double capacity, double q_gen, bool is_fixed, double c_a1, double c_a2, int domain_type, const char* fluid_medium, double fluid_volume) {
    static_cast<ThermalSystem*>(system)->update_node(id, temp, capacity, q_gen, is_fixed, c_a1, c_a2, domain_type, fluid_medium, fluid_volume);
}

EXPORT void set_node_fluid_params(void* system, int id, double mix_ratio, double rho_a0, double rho_a1, double rho_a2, double cp_a0, double cp_a1, double cp_a2) {
    static_cast<ThermalSystem*>(system)->set_node_fluid_params(id, mix_ratio, rho_a0, rho_a1, rho_a2, cp_a0, cp_a1, cp_a2);
}

EXPORT void add_link(void* system, int id, int node_a_id, int node_b_id, int type, double p1, double p2, double g_a1, double g_a2, double fan_area) {
    static_cast<ThermalSystem*>(system)->add_link(id, node_a_id, node_b_id, type, p1, p2, g_a1, g_a2, fan_area);
}

EXPORT void update_link(void* system, int id, double p1, double p2, double g_a1, double g_a2, double fan_area) {
    static_cast<ThermalSystem*>(system)->update_link(id, p1, p2, g_a1, g_a2, fan_area);
}

EXPORT void clear_system(void* system) {
    static_cast<ThermalSystem*>(system)->clear();
}

EXPORT void step_explicit_euler(void* system, double dt) {
    static_cast<ThermalSystem*>(system)->step_explicit_euler(dt);
}

EXPORT void step_rk4(void* system, double dt) {
    static_cast<ThermalSystem*>(system)->step_rk4(dt);
}

EXPORT void step_backward_euler(void* system, double dt, double tolerance, int max_iterations) {
    static_cast<ThermalSystem*>(system)->step_backward_euler(dt, tolerance, max_iterations);
}

EXPORT int solve_steady_state(void* system, double tolerance, int max_iterations) {
    return static_cast<ThermalSystem*>(system)->solve_steady_state(tolerance, max_iterations);
}

EXPORT double get_node_temperature(void* system, int id) {
    return static_cast<ThermalSystem*>(system)->get_node_temperature(id);
}

EXPORT int get_node_count(void* system) {
    return static_cast<ThermalSystem*>(system)->get_node_count();
}

EXPORT int get_nodes_data(void* system, int* ids_out, double* temps_out) {
    ThermalSystem* sys = static_cast<ThermalSystem*>(system);
    const auto& nodes = sys->get_nodes();
    int count = static_cast<int>(nodes.size());
    for (int i = 0; i < count; ++i) {
        ids_out[i] = nodes[i].id;
        temps_out[i] = nodes[i].temperature;
    }
    return count;
}
