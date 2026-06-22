#ifndef SOLVER_H
#define SOLVER_H

#include <vector>
#include <string>
#include <unordered_map>

// Heat transfer link types
enum LinkType {
    LINK_CONDUCTION = 0,
    LINK_CONVECTION = 1,
    LINK_RADIATION  = 2,
    LINK_FLOW       = 3,  // Advection / fluid flow
    LINK_FAN        = 4   // Dynamic pressure-velocity fan/pump flow
};

struct ThermalNode {
    int id;
    std::string name;
    double temperature; // in Kelvin
    double capacity;    // J/K (c_a0)
    double q_gen;       // W
    bool is_fixed;      // Boundary condition (fixed temperature)
    double c_a1 = 0.0;  // J/K^2
    double c_a2 = 0.0;  // J/K^3
    int domain_type = 0; // 0: Solid, 1: Fluid
    std::string fluid_medium = "Water";
    double fluid_volume = 1.0; // in Liters
    double fluid_mix_ratio = 0.5; // 0.0–1.0 glycol fraction for "Mixture"
    double fluid_rho_a0 = 1000.0; // Custom density ρ(T) = a0 + a1*T + a2*T²
    double fluid_rho_a1 = 0.0;
    double fluid_rho_a2 = 0.0;
    double fluid_cp_a0 = 4184.0;  // Custom specific heat cp(T) = a0 + a1*T + a2*T²
    double fluid_cp_a1 = 0.0;
    double fluid_cp_a2 = 0.0;
};

struct ThermalLink {
    int id;
    int node_a_id;
    int node_b_id;
    LinkType type;
    double p1;          // Conduction G, Convection hA, Radiation sigma*epsilon*A, Flow m_dot*cp, or Fan P_max
    double p2;          // Flow direction indicator (1.0/A->B or -1.0/B->A), or Fan v_max
    double g_a1 = 0.0;  // Fan system quadratic coefficient K
    double g_a2 = 0.0;  // Fan system linear coefficient R
    double fan_area = 0.005; // Fan flow area (m^2)
};

class ThermalSystem {
public:
    ThermalSystem() = default;
    ~ThermalSystem() = default;

    void add_node(int id, const char* name, double init_temp, double capacity, double q_gen, bool is_fixed, double c_a1 = 0.0, double c_a2 = 0.0, int domain_type = 0, const char* fluid_medium = "", double fluid_volume = 0.0);
    void add_link(int id, int node_a_id, int node_b_id, int type, double p1, double p2, double g_a1 = 0.0, double g_a2 = 0.0, double fan_area = 0.005);
    
    void update_node(int id, double temp, double capacity, double q_gen, bool is_fixed, double c_a1 = 0.0, double c_a2 = 0.0, int domain_type = 0, const char* fluid_medium = "", double fluid_volume = 0.0);
    void set_node_fluid_params(int id, double mix_ratio, double rho_a0, double rho_a1, double rho_a2, double cp_a0, double cp_a1, double cp_a2);
    void update_link(int id, double p1, double p2, double g_a1 = 0.0, double g_a2 = 0.0, double fan_area = 0.005);
    
    void clear();

    // Numerical Solvers
    void step_explicit_euler(double dt);
    void step_rk4(double dt);
    void step_backward_euler(double dt, double tolerance, int max_iterations);
    int solve_steady_state(double tolerance, int max_iterations);

    // Getters
    double get_node_temperature(int id) const;
    int get_node_count() const { return nodes.size(); }
    int get_link_count() const { return links.size(); }
    
    const std::vector<ThermalNode>& get_nodes() const { return nodes; }

private:
    std::vector<ThermalNode> nodes;
    std::vector<ThermalLink> links;
    
    // Quick lookup maps
    std::unordered_map<int, size_t> node_id_to_index;
    std::unordered_map<int, size_t> link_id_to_index;

    // Computes net heat rate (W) entering node at index node_idx given current temperatures
    double compute_net_heat_rate(size_t node_idx, const std::vector<double>& temps) const;

    void GetNodeCapacityAndDeriv(const ThermalNode& node, double temp_k, double& C, double& dC) const;
    void GetLinkFlowRateAndDeriv(const ThermalLink& link, double T_up_k, double& F, double& dF) const;
};

#endif // SOLVER_H
