#include "solver.h"
#include <cmath>
#include <cstring>
#include <algorithm>

static void GetFluidProperties(const ThermalNode& node, double T_c, double& rho, double& drho, double& cp, double& dcp) {
    const std::string& medium = node.fluid_medium;
    if (medium == "Water") {
        rho = 1000.0 - 0.0178 * T_c - 0.00557 * T_c * T_c + 0.000027 * T_c * T_c * T_c;
        drho = -0.0178 - 0.01114 * T_c + 0.000081 * T_c * T_c;
        
        cp = 4184.0 - 0.09 * T_c + 0.006 * T_c * T_c;
        dcp = -0.09 + 0.012 * T_c;
    }
    else if (medium == "Glycol") { // Water-Glycol 50/50
        rho = 1060.0 - 0.65 * T_c;
        drho = -0.65;
        
        cp = 3300.0 + 3.5 * T_c;
        dcp = 3.5;
    }
    else if (medium == "Oil") { // Engine Oil
        rho = 890.0 - 0.60 * T_c;
        drho = -0.60;
        
        cp = 1800.0 + 3.0 * T_c;
        dcp = 3.0;
    }
    else if (medium == "Air") {
        double denom = T_c + 273.15;
        if (denom < 10.0) denom = 10.0;
        rho = 353.18295 / denom;
        drho = -353.18295 / (denom * denom);
        
        cp = 1005.0 + 0.05 * T_c;
        dcp = 0.05;
    }
    else if (medium == "Mixture") { // Water-Glycol mixture with user-defined ratio
        double r = node.fluid_mix_ratio; // glycol fraction (0.0 = pure water, 1.0 = pure glycol)
        double ir = 1.0 - r;
        
        // Water properties
        double rho_w = 1000.0 - 0.0178 * T_c - 0.00557 * T_c * T_c + 0.000027 * T_c * T_c * T_c;
        double drho_w = -0.0178 - 0.01114 * T_c + 0.000081 * T_c * T_c;
        double cp_w = 4184.0 - 0.09 * T_c + 0.006 * T_c * T_c;
        double dcp_w = -0.09 + 0.012 * T_c;
        
        // Pure Glycol properties
        double rho_g = 1060.0 - 0.65 * T_c;
        double drho_g = -0.65;
        double cp_g = 3300.0 + 3.5 * T_c;
        double dcp_g = 3.5;
        
        // Linear interpolation by mass/volume fraction
        rho = ir * rho_w + r * rho_g;
        drho = ir * drho_w + r * drho_g;
        cp = ir * cp_w + r * cp_g;
        dcp = ir * dcp_w + r * dcp_g;
    }
    else if (medium == "Custom") { // User-defined polynomial coefficients
        rho = node.fluid_rho_a0 + node.fluid_rho_a1 * T_c + node.fluid_rho_a2 * T_c * T_c;
        drho = node.fluid_rho_a1 + 2.0 * node.fluid_rho_a2 * T_c;
        
        cp = node.fluid_cp_a0 + node.fluid_cp_a1 * T_c + node.fluid_cp_a2 * T_c * T_c;
        dcp = node.fluid_cp_a1 + 2.0 * node.fluid_cp_a2 * T_c;
    }
    else { // Fallback (Water-like)
        rho = 1000.0;
        drho = 0.0;
        cp = 4184.0;
        dcp = 0.0;
    }
}

void ThermalSystem::GetNodeCapacityAndDeriv(const ThermalNode& node, double temp_k, double& C, double& dC) const {
    double T_c = temp_k - 273.15;
    if (node.domain_type == 1) { // Fluid Domain
        double rho = 0.0, drho = 0.0;
        double cp = 0.0, dcp = 0.0;
        GetFluidProperties(node, T_c, rho, drho, cp, dcp);
        
        double V_m3 = node.fluid_volume * 1e-3; // volume in Liters
        C = rho * V_m3 * cp;
        dC = V_m3 * (rho * dcp + cp * drho);
        if (C < 0.1) C = 0.1;
    } else { // Solid Domain (Custom or Material)
        C = node.capacity + node.c_a1 * T_c + node.c_a2 * T_c * T_c;
        dC = node.c_a1 + 2.0 * node.c_a2 * T_c;
        if (C < 0.1) C = 0.1;
    }
}

void ThermalSystem::GetLinkFlowRateAndDeriv(const ThermalLink& link, double T_up_k, double& F, double& dF) const {
    double T_up_c = T_up_k - 273.15;
    
    int up_node_id = (link.p2 >= 0.0) ? link.node_a_id : link.node_b_id;
    auto it = node_id_to_index.find(up_node_id);
    
    double vol_rate_lmin = 0.0;
    
    if (link.type == LINK_FLOW || link.type == LINK_FAN) {
        if (link.type == LINK_FLOW) {
            vol_rate_lmin = link.p1;
        } else { // LINK_FAN
            double P_max = link.p1;            // Pa
            double v_max = std::abs(link.p2);   // m/s
            double K = link.g_a1;              // Pa/(m/s)^2
            double R = link.g_a2;              // Pa/(m/s)
            double A = link.fan_area;          // m^2
            
            if (v_max <= 1e-6) v_max = 1.0;
            double B = P_max / (v_max * v_max);
            double denom = K + B;
            double v_oper = 0.0;
            if (denom > 1e-9) {
                double disc = R * R + 4.0 * denom * P_max;
                if (disc >= 0.0) {
                    v_oper = (-R + std::sqrt(disc)) / (2.0 * denom);
                }
            }
            vol_rate_lmin = v_oper * A * 60000.0; // L/min
        }
        
        if (it != node_id_to_index.end()) {
            const ThermalNode& up_node = nodes[it->second];
            if (up_node.domain_type == 1) { // Upstream node is Fluid Domain
                double rho = 0.0, drho = 0.0;
                double cp = 0.0, dcp = 0.0;
                GetFluidProperties(up_node, T_up_c, rho, drho, cp, dcp);
                
                double vol_m3_s = vol_rate_lmin * 1.66667e-5; // L/min to m^3/s
                F = rho * vol_m3_s * cp;
                dF = vol_m3_s * (rho * dcp + cp * drho);
                return;
            }
        }
    }
    
    // Fallback: Solid or not found, use link's own polynomial parameters
    F = link.p1 + link.g_a1 * T_up_c + link.g_a2 * T_up_c * T_up_c;
    dF = link.g_a1 + 2.0 * link.g_a2 * T_up_c;
}

void ThermalSystem::add_node(int id, const char* name, double init_temp, double capacity, double q_gen, bool is_fixed, double c_a1, double c_a2, int domain_type, const char* fluid_medium, double fluid_volume) {
    if (node_id_to_index.find(id) != node_id_to_index.end()) {
        update_node(id, init_temp, capacity, q_gen, is_fixed, c_a1, c_a2, domain_type, fluid_medium, fluid_volume);
        return;
    }
    
    ThermalNode node;
    node.id = id;
    node.name = name ? name : "Node";
    node.temperature = init_temp;
    node.capacity = capacity > 0.0 ? capacity : 1.0; // Avoid division by zero
    node.q_gen = q_gen;
    node.is_fixed = is_fixed;
    node.c_a1 = c_a1;
    node.c_a2 = c_a2;
    node.domain_type = domain_type;
    node.fluid_medium = fluid_medium ? fluid_medium : "";
    node.fluid_volume = fluid_volume;

    node_id_to_index[id] = nodes.size();
    nodes.push_back(node);
}

void ThermalSystem::update_node(int id, double temp, double capacity, double q_gen, bool is_fixed, double c_a1, double c_a2, int domain_type, const char* fluid_medium, double fluid_volume) {
    auto it = node_id_to_index.find(id);
    if (it != node_id_to_index.end()) {
        size_t idx = it->second;
        nodes[idx].temperature = temp;
        nodes[idx].capacity = capacity > 0.0 ? capacity : 1.0;
        nodes[idx].q_gen = q_gen;
        nodes[idx].is_fixed = is_fixed;
        nodes[idx].c_a1 = c_a1;
        nodes[idx].c_a2 = c_a2;
        nodes[idx].domain_type = domain_type;
        if (fluid_medium) {
            nodes[idx].fluid_medium = fluid_medium;
        }
        nodes[idx].fluid_volume = fluid_volume;
    }
}

void ThermalSystem::set_node_fluid_params(int id, double mix_ratio, double rho_a0, double rho_a1, double rho_a2, double cp_a0, double cp_a1, double cp_a2) {
    auto it = node_id_to_index.find(id);
    if (it != node_id_to_index.end()) {
        size_t idx = it->second;
        nodes[idx].fluid_mix_ratio = mix_ratio;
        nodes[idx].fluid_rho_a0 = rho_a0;
        nodes[idx].fluid_rho_a1 = rho_a1;
        nodes[idx].fluid_rho_a2 = rho_a2;
        nodes[idx].fluid_cp_a0 = cp_a0;
        nodes[idx].fluid_cp_a1 = cp_a1;
        nodes[idx].fluid_cp_a2 = cp_a2;
    }
}

void ThermalSystem::add_link(int id, int node_a_id, int node_b_id, int type, double p1, double p2, double g_a1, double g_a2, double fan_area) {
    if (link_id_to_index.find(id) != link_id_to_index.end()) {
        update_link(id, p1, p2, g_a1, g_a2, fan_area);
        return;
    }

    ThermalLink link;
    link.id = id;
    link.node_a_id = node_a_id;
    link.node_b_id = node_b_id;
    link.type = static_cast<LinkType>(type);
    link.p1 = p1;
    link.p2 = p2;
    link.g_a1 = g_a1;
    link.g_a2 = g_a2;
    link.fan_area = fan_area;

    link_id_to_index[id] = links.size();
    links.push_back(link);
}

void ThermalSystem::update_link(int id, double p1, double p2, double g_a1, double g_a2, double fan_area) {
    auto it = link_id_to_index.find(id);
    if (it != link_id_to_index.end()) {
        size_t idx = it->second;
        links[idx].p1 = p1;
        links[idx].p2 = p2;
        links[idx].g_a1 = g_a1;
        links[idx].g_a2 = g_a2;
        links[idx].fan_area = fan_area;
    }
}

void ThermalSystem::clear() {
    nodes.clear();
    links.clear();
    node_id_to_index.clear();
    link_id_to_index.clear();
}

double ThermalSystem::get_node_temperature(int id) const {
    auto it = node_id_to_index.find(id);
    if (it != node_id_to_index.end()) {
        return nodes[it->second].temperature;
    }
    return 0.0;
}

double ThermalSystem::compute_net_heat_rate(size_t node_idx, const std::vector<double>& temps) const {
    const ThermalNode& node = nodes[node_idx];
    if (node.is_fixed) {
        return 0.0;
    }

    double q_net = node.q_gen;
    int this_id = node.id;
    double T_this = temps[node_idx];

    for (const auto& link : links) {
        bool connected = false;
        int other_id = -1;
        bool is_node_a = false;

        if (link.node_a_id == this_id) {
            connected = true;
            other_id = link.node_b_id;
            is_node_a = true;
        } else if (link.node_b_id == this_id) {
            connected = true;
            other_id = link.node_a_id;
            is_node_a = false;
        }

        if (!connected) continue;

        auto other_it = node_id_to_index.find(other_id);
        if (other_it == node_id_to_index.end()) continue;
        size_t other_idx = other_it->second;
        double T_other = temps[other_idx];

        switch (link.type) {
            case LINK_CONDUCTION:
            case LINK_CONVECTION: {
                double T_avg = 0.5 * (T_this + T_other);
                double T_avg_c = T_avg - 273.15;
                double G = link.p1 + link.g_a1 * T_avg_c + link.g_a2 * T_avg_c * T_avg_c;
                q_net += G * (T_other - T_this);
                break;
            }
            case LINK_RADIATION: {
                double T_other_k = std::max(T_other, 0.0);
                double T_this_k = std::max(T_this, 0.0);
                double T_avg = 0.5 * (T_this_k + T_other_k);
                double T_avg_c = T_avg - 273.15;
                double G_rad = link.p1 + link.g_a1 * T_avg_c + link.g_a2 * T_avg_c * T_avg_c;
                q_net += G_rad * (std::pow(T_other_k, 4) - std::pow(T_this_k, 4));
                break;
            }
            case LINK_FAN:
            case LINK_FLOW: {
                double dir = link.p2;
                if (dir == 0.0) {
                    break;
                }

                bool is_upstream = (is_node_a && dir > 0.0) || (!is_node_a && dir < 0.0);
                double T_up = is_upstream ? T_this : T_other;
                T_up = std::max(T_up, 0.0);
                
                double flow_rate = 0.0, dflow_rate = 0.0;
                GetLinkFlowRateAndDeriv(link, T_up, flow_rate, dflow_rate);

                if (is_upstream) {
                    q_net -= flow_rate * T_this;
                } else {
                    q_net += flow_rate * T_other;
                }
                break;
            }
        }
    }

    return q_net;
}

void ThermalSystem::step_explicit_euler(double dt) {
    if (nodes.empty()) return;

    std::vector<double> current_temps(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i) {
        current_temps[i] = nodes[i].temperature;
    }

    std::vector<double> next_temps = current_temps;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].is_fixed) continue;
        double q_net = compute_net_heat_rate(i, current_temps);
        double C = 0.0, dC = 0.0;
        GetNodeCapacityAndDeriv(nodes[i], current_temps[i], C, dC);
        next_temps[i] = current_temps[i] + (dt / C) * q_net;
    }

    for (size_t i = 0; i < nodes.size(); ++i) {
        nodes[i].temperature = next_temps[i];
    }
}

void ThermalSystem::step_rk4(double dt) {
    if (nodes.empty()) return;

    size_t n = nodes.size();
    std::vector<double> T0(n);
    for (size_t i = 0; i < n; ++i) {
        T0[i] = nodes[i].temperature;
    }

    // k1
    std::vector<double> k1(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        if (!nodes[i].is_fixed) {
            double C = 0.0, dC = 0.0;
            GetNodeCapacityAndDeriv(nodes[i], T0[i], C, dC);
            k1[i] = compute_net_heat_rate(i, T0) / C;
        }
    }

    // T1
    std::vector<double> T1(n);
    for (size_t i = 0; i < n; ++i) {
        T1[i] = nodes[i].is_fixed ? T0[i] : T0[i] + 0.5 * dt * k1[i];
    }

    // k2
    std::vector<double> k2(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        if (!nodes[i].is_fixed) {
            double C = 0.0, dC = 0.0;
            GetNodeCapacityAndDeriv(nodes[i], T1[i], C, dC);
            k2[i] = compute_net_heat_rate(i, T1) / C;
        }
    }

    // T2
    std::vector<double> T2(n);
    for (size_t i = 0; i < n; ++i) {
        T2[i] = nodes[i].is_fixed ? T0[i] : T0[i] + 0.5 * dt * k2[i];
    }

    // k3
    std::vector<double> k3(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        if (!nodes[i].is_fixed) {
            double C = 0.0, dC = 0.0;
            GetNodeCapacityAndDeriv(nodes[i], T2[i], C, dC);
            k3[i] = compute_net_heat_rate(i, T2) / C;
        }
    }

    // T3
    std::vector<double> T3(n);
    for (size_t i = 0; i < n; ++i) {
        T3[i] = nodes[i].is_fixed ? T0[i] : T0[i] + dt * k3[i];
    }

    // k4
    std::vector<double> k4(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        if (!nodes[i].is_fixed) {
            double C = 0.0, dC = 0.0;
            GetNodeCapacityAndDeriv(nodes[i], T3[i], C, dC);
            k4[i] = compute_net_heat_rate(i, T3) / C;
        }
    }

    // Final update
    for (size_t i = 0; i < n; ++i) {
        if (nodes[i].is_fixed) continue;
        nodes[i].temperature = T0[i] + (dt / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }
}

int ThermalSystem::solve_steady_state(double tolerance, int max_iterations) {
    if (nodes.empty()) return 0;

    size_t n = nodes.size();
    int iter = 0;
    bool converged = false;

    while (iter < max_iterations && !converged) {
        double max_diff = 0.0;
        converged = true;

        // Perform one Gauss-Seidel sweep
        for (size_t i = 0; i < n; ++i) {
            if (nodes[i].is_fixed) continue;

            // Formulate: A - B * T_i - D * T_i^4 = 0
            // where:
            // A = q_gen + sum(G_j * T_j) + sum(F_in * T_in) + sum(R_j * T_j^4)
            // B = sum(G_j) + sum(F_out)
            // D = sum(R_j)
            
            double A = nodes[i].q_gen;
            double B = 0.0;
            double D = 0.0;
            int this_id = nodes[i].id;

            for (const auto& link : links) {
                bool connected = false;
                int other_id = -1;
                bool is_node_a = false;

                if (link.node_a_id == this_id) {
                    connected = true;
                    other_id = link.node_b_id;
                    is_node_a = true;
                } else if (link.node_b_id == this_id) {
                    connected = true;
                    other_id = link.node_a_id;
                    is_node_a = false;
                }

                if (!connected) continue;

                auto other_it = node_id_to_index.find(other_id);
                if (other_it == node_id_to_index.end()) continue;
                size_t other_idx = other_it->second;
                double T_other = nodes[other_idx].temperature;
                double T_this = nodes[i].temperature;

                switch (link.type) {
                    case LINK_CONDUCTION:
                    case LINK_CONVECTION: {
                        double T_avg = 0.5 * (T_this + T_other);
                        double T_avg_c = T_avg - 273.15;
                        double G = link.p1 + link.g_a1 * T_avg_c + link.g_a2 * T_avg_c * T_avg_c;
                        A += G * T_other;
                        B += G;
                        break;
                    }
                    case LINK_RADIATION: {
                        double T_this_k = std::max(T_this, 0.0);
                        double T_other_k = std::max(T_other, 0.0);
                        double T_avg = 0.5 * (T_this_k + T_other_k);
                        double T_avg_c = T_avg - 273.15;
                        double G_rad = link.p1 + link.g_a1 * T_avg_c + link.g_a2 * T_avg_c * T_avg_c;
                        A += G_rad * std::pow(T_other_k, 4);
                        D += G_rad;
                        break;
                    }
                    case LINK_FAN:
                    case LINK_FLOW: {
                        double dir = link.p2;
                        if (dir == 0.0) {
                            break;
                        }

                        bool is_upstream = (is_node_a && dir > 0.0) || (!is_node_a && dir < 0.0);
                        double T_up = is_upstream ? T_this : T_other;
                        T_up = std::max(T_up, 0.0);

                        double flow_rate = 0.0, dflow_rate = 0.0;
                        GetLinkFlowRateAndDeriv(link, T_up, flow_rate, dflow_rate);

                        if (is_upstream) {
                            // Fluid leaves this node: linear term in temperature
                            B += flow_rate;
                        } else {
                            // Fluid enters this node: input term
                            A += flow_rate * T_other;
                        }
                        break;
                    }
                }
            }

            double T_old = nodes[i].temperature;
            double T_new = T_old;

            if (D == 0.0) {
                // Linear system isolation
                if (B > 0.0) {
                    T_new = A / B;
                }
            } else {
                // Non-linear Newton solver for T_new
                // f(T) = A - B*T - D*T^4
                // f'(T) = -B - 4*D*T^3
                // We run up to 10 Newton iterations
                double T_curr = std::max(T_old, 0.1); // Avoid starting at absolute zero
                for (int n_iter = 0; n_iter < 10; ++n_iter) {
                    double f_val = A - B * T_curr - D * std::pow(T_curr, 4);
                    double f_prime = -B - 4.0 * D * std::pow(T_curr, 3);
                    double step = f_val / f_prime;
                    T_curr -= step;
                    if (T_curr < 0.0) T_curr = 0.1; // Bound check
                    if (std::abs(step) < 1e-6) break;
                }
                T_new = T_curr;
            }

            nodes[i].temperature = T_new;
            
            double diff = std::abs(T_new - T_old);
            if (diff > max_diff) {
                max_diff = diff;
            }
            if (diff > tolerance) {
                converged = false;
            }
        }

        iter++;
    }

    return iter;
}

bool solve_linear_system(std::vector<std::vector<double>>& J, std::vector<double>& b) {
    size_t n = b.size();
    for (size_t i = 0; i < n; ++i) {
        // Find pivot
        size_t pivot = i;
        double max_val = std::abs(J[i][i]);
        for (size_t r = i + 1; r < n; ++r) {
            if (std::abs(J[r][i]) > max_val) {
                max_val = std::abs(J[r][i]);
                pivot = r;
            }
        }
        
        // Swap rows
        if (pivot != i) {
            std::swap(J[i], J[pivot]);
            std::swap(b[i], b[pivot]);
        }
        
        // Check singularity
        if (std::abs(J[i][i]) < 1e-12) {
            return false; // Singular matrix
        }
        
        // Eliminate below
        for (size_t r = i + 1; r < n; ++r) {
            double factor = J[r][i] / J[i][i];
            for (size_t c = i; c < n; ++c) {
                J[r][c] -= factor * J[i][c];
            }
            b[r] -= factor * b[i];
        }
    }
    
    // Back substitution
    for (int i = (int)n - 1; i >= 0; --i) {
        double sum = 0.0;
        for (size_t c = i + 1; c < n; ++c) {
            sum += J[i][c] * b[c];
        }
        b[i] = (b[i] - sum) / J[i][i];
    }
    return true;
}

void ThermalSystem::step_backward_euler(double dt, double tolerance, int max_iterations) {
    if (nodes.empty()) return;

    size_t n = nodes.size();
    std::vector<double> T_prev(n);
    for (size_t i = 0; i < n; ++i) {
        T_prev[i] = nodes[i].temperature;
    }

    std::vector<double> T_curr = T_prev;

    for (int iter = 0; iter < max_iterations; ++iter) {
        std::vector<double> R(n, 0.0);
        std::vector<std::vector<double>> J(n, std::vector<double>(n, 0.0));

        // 1. Build initial Residual and Jacobian diagonal from nodes
        for (size_t i = 0; i < n; ++i) {
            if (nodes[i].is_fixed) {
                R[i] = T_curr[i] - T_prev[i];
                J[i][i] = 1.0;
            } else {
                double C = nodes[i].capacity + nodes[i].c_a1 * T_curr[i] + nodes[i].c_a2 * T_curr[i] * T_curr[i];
                double dC = nodes[i].c_a1 + 2.0 * nodes[i].c_a2 * T_curr[i];
                if (C < 0.1) C = 0.1;

                R[i] = C * (T_curr[i] - T_prev[i]) - dt * nodes[i].q_gen;
                J[i][i] = C + dC * (T_curr[i] - T_prev[i]);
            }
        }

        // 2. Add link contributions
        for (const auto& link : links) {
            auto itA = node_id_to_index.find(link.node_a_id);
            auto itB = node_id_to_index.find(link.node_b_id);
            if (itA == node_id_to_index.end() || itB == node_id_to_index.end()) continue;

            size_t idxA = itA->second;
            size_t idxB = itB->second;

            double T_A = T_curr[idxA];
            double T_B = T_curr[idxB];

            double Q = 0.0;
            double dQ_dTA = 0.0;
            double dQ_dTB = 0.0;

            if (link.type == LINK_CONDUCTION || link.type == LINK_CONVECTION) {
                double T_avg = 0.5 * (T_A + T_B);
                double G = link.p1 + link.g_a1 * T_avg + link.g_a2 * T_avg * T_avg;
                double dG = link.g_a1 + 2.0 * link.g_a2 * T_avg;

                Q = G * (T_B - T_A);
                dQ_dTA = 0.5 * dG * (T_B - T_A) - G;
                dQ_dTB = 0.5 * dG * (T_B - T_A) + G;
            }
            else if (link.type == LINK_RADIATION) {
                double TA_k = std::max(T_A, 0.0);
                double TB_k = std::max(T_B, 0.0);
                double T_avg = 0.5 * (TA_k + TB_k);
                double R_rad = link.p1 + link.g_a1 * T_avg + link.g_a2 * T_avg * T_avg;
                double dR = link.g_a1 + 2.0 * link.g_a2 * T_avg;

                Q = R_rad * (std::pow(TB_k, 4) - std::pow(TA_k, 4));
                dQ_dTA = 0.5 * dR * (std::pow(TB_k, 4) - std::pow(TA_k, 4)) - 4.0 * R_rad * std::pow(TA_k, 3);
                dQ_dTB = 0.5 * dR * (std::pow(TB_k, 4) - std::pow(TA_k, 4)) + 4.0 * R_rad * std::pow(TB_k, 3);
            }
            else if (link.type == LINK_FLOW || link.type == LINK_FAN) {
                double dir = link.p2;
                if (dir != 0.0) {
                    bool is_upstream = (dir > 0.0); // A is upstream if dir > 0
                    double T_up = is_upstream ? T_A : T_B;
                    T_up = std::max(T_up, 0.0);
                    
                    double F = 0.0, dF = 0.0;
                    GetLinkFlowRateAndDeriv(link, T_up, F, dF);

                    if (is_upstream) {
                        Q = -F * T_A;
                        dQ_dTA = -(F + dF * T_A);
                        dQ_dTB = 0.0;
                    } else {
                        Q = F * T_B;
                        dQ_dTA = 0.0;
                        dQ_dTB = F + dF * T_B;
                    }
                }
            }

            // Apply contributions
            if (!nodes[idxA].is_fixed) {
                R[idxA] -= dt * Q;
                J[idxA][idxA] -= dt * dQ_dTA;
                J[idxA][idxB] -= dt * dQ_dTB;
            }
            if (!nodes[idxB].is_fixed) {
                R[idxB] -= dt * (-Q);
                J[idxB][idxA] -= dt * (-dQ_dTA);
                J[idxB][idxB] -= dt * (-dQ_dTB);
            }
        }

        // 3. Solve J * delta_T = -R
        std::vector<double> b(n);
        for (size_t i = 0; i < n; ++i) {
            b[i] = -R[i];
        }

        if (!solve_linear_system(J, b)) {
            break; // Singular matrix, stop iterating
        }

        double max_delta = 0.0;
        for (size_t i = 0; i < n; ++i) {
            if (!nodes[i].is_fixed) {
                T_curr[i] += b[i];
                if (T_curr[i] < 0.0) T_curr[i] = 0.0; // Enforce physical temperatures >= 0 K
                max_delta = std::max(max_delta, std::abs(b[i]));
            }
        }

        if (max_delta < tolerance) {
            break; // Converged!
        }
    }

    for (size_t i = 0; i < n; ++i) {
        nodes[i].temperature = T_curr[i];
    }
}

