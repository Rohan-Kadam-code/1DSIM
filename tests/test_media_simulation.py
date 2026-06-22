import sys
import os
import math

# Add src to python path
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src"))

from wrapper.thermal_solver import ThermalSystem, c_to_k, k_to_c

def run_capacity_test(medium, init_temp_c, expected_capacity, volume_l=1.0, q_gen=10000.0, dt=0.01):
    sys_sim = ThermalSystem()
    
    # Add node with fluid domain
    sys_sim.add_node(
        node_id=1,
        name=f"Fluid_{medium}",
        init_temp_k=c_to_k(init_temp_c),
        capacity=1.0,  # capacity is computed dynamically so this initial value is overwritten
        q_gen=q_gen,
        is_fixed=False,
        c_a1=0.0,
        c_a2=0.0,
        domain_type=1,  # Fluid Domain
        fluid_medium=medium,
        fluid_volume=volume_l
    )
    
    # Run 1 step of Explicit Euler to back-calculate capacity
    sys_sim.step_explicit_euler(dt)
    
    new_temp_c = k_to_c(sys_sim.get_node_temperature(1))
    temp_diff = new_temp_c - init_temp_c
    
    # dT/dt = Q / C => C = Q * dt / dT
    sim_capacity = q_gen * dt / temp_diff
    
    print(f"\n--- {medium} at {init_temp_c} C ---")
    print(f"Initial Temp: {init_temp_c} C")
    print(f"New Temp: {new_temp_c:.6f} C (diff = {temp_diff:.6f} C)")
    print(f"Analytical Capacity: {expected_capacity:.4f} J/K")
    print(f"Simulated Capacity:  {sim_capacity:.4f} J/K")
    
    error = abs(sim_capacity - expected_capacity) / expected_capacity
    print(f"Relative Error: {error * 100:.6f} %")
    
    assert error < 1e-4, f"Capacity for {medium} matches analytical value"

def test_fluid_capacities():
    # 1. Water at 25 C
    # rho = 1000.0 - 0.0178 * 25 - 0.00557 * 25^2 + 0.000027 * 25^3 = 996.495625 kg/m3
    # cp = 4184.0 - 0.09 * 25 + 0.006 * 25^2 = 4185.5 J/kgK
    # C = rho * 1e-3 * cp = 4170.832439 J/K
    run_capacity_test("Water", 25.0, 4170.832439)
    
    # 2. Water-Glycol 50/50 at 25 C
    # rho = 1060.0 - 0.65 * 25 = 1043.75 kg/m3
    # cp = 3300.0 + 3.5 * 25 = 3387.5 J/kgK
    # C = rho * 1e-3 * cp = 3535.703125 J/K
    run_capacity_test("Glycol", 25.0, 3535.703125)
    
    # 3. Engine Oil at 25 C
    # rho = 890.0 - 0.6 * 25 = 875.0 kg/m3
    # cp = 1800.0 + 3.0 * 25 = 1875.0 J/kgK
    # C = rho * 1e-3 * cp = 1640.625 J/K
    run_capacity_test("Oil", 25.0, 1640.625)
    
    # 4. Air at 25 C
    # rho = 353.18295 / (25 + 273.15) = 1.18458142 kg/m3
    # cp = 1005.0 + 0.05 * 25 = 1006.25 J/kgK
    # C = rho * 1e-3 * cp = 1.191985055 J/K
    # Since capacity is very small, we use smaller heat source (q_gen = 5 W)
    run_capacity_test("Air", 25.0, 1.191985055, q_gen=5.0, dt=0.01)

def test_fluid_advection():
    print("\n--- Running fluid advection link test ---")
    sys_sim = ThermalSystem()
    
    # Node 1: Fluid (Water) volume = 10 Liters, fixed at 80 C
    sys_sim.add_node(1, "HotFluid", c_to_k(80.0), 10.0, q_gen=0.0, is_fixed=True,
                     domain_type=1, fluid_medium="Water", fluid_volume=10.0)
    
    # Node 2: Fluid (Water) volume = 2 Liters, transient, init at 20 C
    sys_sim.add_node(2, "ColdFluid", c_to_k(20.0), 10.0, q_gen=0.0, is_fixed=False,
                     domain_type=1, fluid_medium="Water", fluid_volume=2.0)
    
    # Flow Link: 10 L/min from Node 1 -> Node 2
    sys_sim.add_link(101, 1, 2, 3, 10.0, 1.0)
    # Return flow Link: 10 L/min from Node 2 -> Node 1
    sys_sim.add_link(102, 2, 1, 3, 10.0, 1.0)
    
    # With a closed loop (link 101: 1->2 and link 102: 2->1), Node 2 sees:
    #   From link 101: +F_up1 * T_1  (incoming from hot upstream Node 1)
    #   From link 102: -F_up2 * T_2  (outgoing, Node 2 is upstream of this return link)
    # 
    # Since both links use the same volumetric flow rate (10 L/min):
    #   F_up1 = rho(T_1) * vol_rate * cp(T_1) (evaluated at upstream T_1 = 80 C)
    #   F_up2 = rho(T_2) * vol_rate * cp(T_2) (evaluated at upstream T_2 = 20 C)
    #
    # At t=0: T_1 = 353.15 K, T_2 = 293.15 K
    # rho(80) = 1000 - 0.0178*80 - 0.00557*6400 + 0.000027*512000
    #         = 1000 - 1.424 - 35.648 + 13.824 = 976.752 kg/m3
    # cp(80)  = 4184 - 0.09*80 + 0.006*6400 = 4215.2 J/(kgK)
    # vol_rate = 10 L/min * 1.66667e-5 = 1.66667e-4 m3/s
    # F_up1 = 976.752 * 1.66667e-4 * 4215.2
    #
    # rho(20) = 1000 - 0.0178*20 - 0.00557*400 + 0.000027*8000
    #         = 1000 - 0.356 - 2.228 + 0.216 = 997.632 kg/m3
    # cp(20)  = 4184 - 0.09*20 + 0.006*400 = 4184.6 J/(kgK)
    # F_up2 = 997.632 * 1.66667e-4 * 4184.6
    #
    # Net Q to Node 2 = F_up1 * T_1_K - F_up2 * T_2_K
    # C_2 = rho(20) * 2e-3 * cp(20) = 997.632 * 0.002 * 4184.6 = 8348.80 J/K
    # dT2/dt = (F_up1 * T_1_K - F_up2 * T_2_K) / C_2
    
    vol_rate = 10.0 * 1.66667e-5
    
    rho_80 = 1000.0 - 0.0178*80 - 0.00557*80*80 + 0.000027*80*80*80
    cp_80  = 4184.0 - 0.09*80 + 0.006*80*80
    F_up1  = rho_80 * vol_rate * cp_80
    
    rho_20 = 1000.0 - 0.0178*20 - 0.00557*20*20 + 0.000027*20*20*20
    cp_20  = 4184.0 - 0.09*20 + 0.006*20*20
    F_up2  = rho_20 * vol_rate * cp_20
    
    T1_K = c_to_k(80.0)
    T2_K = c_to_k(20.0)
    C_2  = rho_20 * 2e-3 * cp_20
    
    Q_net = F_up1 * T1_K - F_up2 * T2_K
    expected_heat_rate = Q_net / C_2
    
    dt = 0.001
    sys_sim.step_explicit_euler(dt)
    
    t2_new = k_to_c(sys_sim.get_node_temperature(2))
    dt2 = t2_new - 20.0
    sim_heat_rate = dt2 / dt
    
    print(f"F_up1 (at 80C): {F_up1:.4f} W/K")
    print(f"F_up2 (at 20C): {F_up2:.4f} W/K")
    print(f"C_2: {C_2:.4f} J/K")
    print(f"Expected dT2/dt: {expected_heat_rate:.4f} C/s")
    print(f"Simulated dT2/dt: {sim_heat_rate:.4f} C/s")
    error = abs(sim_heat_rate - expected_heat_rate) / abs(expected_heat_rate)
    print(f"Relative Error: {error * 100:.6f} %")
    
    assert error < 1e-3, "Advective flow heat transfer matches analytical upstream properties"
    print("Fluid advection link test passed successfully!")

def test_mixture_fluid():
    print("\n--- Running Mixture fluid capacity test ---")
    sys_sim = ThermalSystem()
    sys_sim.add_node(1, "MixtureFluid", c_to_k(25.0), 1.0, q_gen=10000.0, is_fixed=False,
                     domain_type=1, fluid_medium="Mixture", fluid_volume=1.0)
    # Set mix ratio to 0.4
    sys_sim.set_node_fluid_params(1, 0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    
    sys_sim.step_explicit_euler(0.01)
    new_temp_c = k_to_c(sys_sim.get_node_temperature(1))
    temp_diff = new_temp_c - 25.0
    sim_capacity = 10000.0 * 0.01 / temp_diff
    
    # Calculate expected
    rho_w = 1000.0 - 0.0178 * 25.0 - 0.00557 * 25.0**2 + 0.000027 * 25.0**3
    cp_w = 4184.0 - 0.09 * 25.0 + 0.006 * 25.0**2
    
    rho_g = 1060.0 - 0.65 * 25.0
    cp_g = 3300.0 + 3.5 * 25.0
    
    r = 0.4
    ir = 1.0 - r
    rho_expected = ir * rho_w + r * rho_g
    cp_expected = ir * cp_w + r * cp_g
    C_expected = rho_expected * 1e-3 * cp_expected
    
    print(f"Analytical Capacity: {C_expected:.4f} J/K")
    print(f"Simulated Capacity:  {sim_capacity:.4f} J/K")
    error = abs(sim_capacity - C_expected) / C_expected
    print(f"Relative Error: {error * 100:.6f} %")
    assert error < 1e-4, "Mixture fluid capacity matches analytical value"
    print("Mixture fluid capacity test passed successfully!")

def test_custom_fluid():
    print("\n--- Running Custom fluid capacity test ---")
    sys_sim = ThermalSystem()
    sys_sim.add_node(1, "CustomFluid", c_to_k(25.0), 1.0, q_gen=10000.0, is_fixed=False,
                     domain_type=1, fluid_medium="Custom", fluid_volume=1.0)
    # Set custom parameters (rho: a0=900, a1=-0.5, a2=0.001; cp: a0=2000, a1=2.5, a2=-0.01)
    sys_sim.set_node_fluid_params(1, 0.0, 900.0, -0.5, 0.001, 2000.0, 2.5, -0.01)
    
    sys_sim.step_explicit_euler(0.01)
    new_temp_c = k_to_c(sys_sim.get_node_temperature(1))
    temp_diff = new_temp_c - 25.0
    sim_capacity = 10000.0 * 0.01 / temp_diff
    
    rho_expected = 900.0 - 0.5 * 25.0 + 0.001 * 25.0**2
    cp_expected = 2000.0 + 2.5 * 25.0 - 0.01 * 25.0**2
    C_expected = rho_expected * 1e-3 * cp_expected
    
    print(f"Analytical Capacity: {C_expected:.4f} J/K")
    print(f"Simulated Capacity:  {sim_capacity:.4f} J/K")
    error = abs(sim_capacity - C_expected) / C_expected
    print(f"Relative Error: {error * 100:.6f} %")
    assert error < 1e-4, "Custom fluid capacity matches analytical value"
    print("Custom fluid capacity test passed successfully!")

def test_fan_link():
    print("\n--- Running Fan Link test ---")
    sys_sim = ThermalSystem()
    
    # Node 1: Fluid (Water) volume = 10 Liters, fixed at 80 C
    sys_sim.add_node(1, "HotFluid", c_to_k(80.0), 10.0, q_gen=0.0, is_fixed=True,
                     domain_type=1, fluid_medium="Water", fluid_volume=10.0)
    
    # Node 2: Fluid (Water) volume = 2 Liters, transient, init at 20 C
    sys_sim.add_node(2, "ColdFluid", c_to_k(20.0), 10.0, q_gen=0.0, is_fixed=False,
                     domain_type=1, fluid_medium="Water", fluid_volume=2.0)
    
    # Fan Link: 101 connecting Node 1 -> Node 2
    # P_max = 200.0, v_max = 10.0, K = 0.5, R = 2.0, area = 0.005
    # Direction positive (10.0) means flow is from Node 1 -> Node 2.
    sys_sim.add_link(101, 1, 2, 4, p1=200.0, p2=10.0, g_a1=0.5, g_a2=2.0, fan_area=0.005)
    
    # Return Flow Link: 102 from Node 2 -> Node 1 (fixed flow, matching the analytical fan flow rate)
    v_oper = (-2.0 + math.sqrt(2.0**2 + 4.0 * (0.5 + 200.0/100.0) * 200.0)) / (2.0 * (0.5 + 200.0/100.0))
    vol_rate_lmin = v_oper * 0.005 * 60000.0
    
    sys_sim.add_link(102, 2, 1, 3, vol_rate_lmin, 1.0)
    
    vol_rate = vol_rate_lmin * 1.66667e-5
    
    rho_80 = 1000.0 - 0.0178*80 - 0.00557*80*80 + 0.000027*80*80*80
    cp_80  = 4184.0 - 0.09*80 + 0.006*80*80
    F_up1  = rho_80 * vol_rate * cp_80
    
    rho_20 = 1000.0 - 0.0178*20 - 0.00557*20*20 + 0.000027*20*20*20
    cp_20  = 4184.0 - 0.09*20 + 0.006*20*20
    F_up2  = rho_20 * vol_rate * cp_20
    
    T1_K = c_to_k(80.0)
    T2_K = c_to_k(20.0)
    C_2  = rho_20 * 2e-3 * cp_20
    
    Q_net = F_up1 * T1_K - F_up2 * T2_K
    expected_heat_rate = Q_net / C_2
    
    dt = 0.001
    sys_sim.step_explicit_euler(dt)
    
    t2_new = k_to_c(sys_sim.get_node_temperature(2))
    dt2 = t2_new - 20.0
    sim_heat_rate = dt2 / dt
    
    print(f"v_oper: {v_oper:.6f} m/s")
    print(f"Flow rate: {vol_rate_lmin:.4f} L/min")
    print(f"Expected dT2/dt: {expected_heat_rate:.4f} C/s")
    print(f"Simulated dT2/dt: {sim_heat_rate:.4f} C/s")
    error = abs(sim_heat_rate - expected_heat_rate) / abs(expected_heat_rate)
    print(f"Relative Error: {error * 100:.6f} %")
    
    assert error < 1e-3, "Fan advective flow heat transfer matches analytical root flow rate"
    print("Fan Link test passed successfully!")

if __name__ == "__main__":
    try:
        test_fluid_capacities()
        test_fluid_advection()
        test_mixture_fluid()
        test_custom_fluid()
        test_fan_link()
        print("\nAll media simulation solver tests passed successfully!")
    except AssertionError as ae:
        print(f"\nAssertion Error: {ae}")
        sys.exit(1)
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        sys.exit(1)

