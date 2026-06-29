import sys
import os
import math

# Add src to python path
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src"))

from L4_sync.thermal_solver import ThermalSystem, c_to_k, k_to_c

def test_transient_cooling():
    print("Running transient cooling test...")
    sys_sim = ThermalSystem()
    
    # C = 1000 J/K, init_temp = 100 C, fixed = False
    sys_sim.add_node(1, "HotMass", c_to_k(100.0), 1000.0, q_gen=0.0, is_fixed=False)
    # Ambient fixed at 20 C
    sys_sim.add_node(2, "Ambient", c_to_k(20.0), 1.0, q_gen=0.0, is_fixed=True)
    
    # Conduction Link: G = 10 W/K
    sys_sim.add_link(101, 1, 2, 0, 10.0) # type 0 = Conduction
    
    # Analytical solution: T(t) = T_amb + (T_init - T_amb) * exp(-t / tau)
    # where tau = C / G = 1000 / 10 = 100 seconds
    # At t = 50 seconds:
    t_target = 50.0
    expected_temp_c = 20.0 + (100.0 - 20.0) * math.exp(-t_target / 100.0) # ~ 68.524 C
    
    # Run simulation with small time step
    dt = 0.1
    steps = int(t_target / dt)
    for _ in range(steps):
        sys_sim.step_rk4(dt)
        
    sim_temp_c = k_to_c(sys_sim.get_node_temperature(1))
    
    print(f"Target Time: {t_target}s")
    print(f"Analytical Temp: {expected_temp_c:.4f} C")
    print(f"Simulated RK4 Temp: {sim_temp_c:.4f} C")
    
    diff = abs(sim_temp_c - expected_temp_c)
    print(f"Difference: {diff:.6f} C")
    
    assert diff < 1e-3, "RK4 simulation matches analytical solution"
    print("Transient cooling test passed successfully!")

def test_steady_state():
    print("\nRunning steady-state heat balance test...")
    sys_sim = ThermalSystem()
    
    # Heat Source Node (C is arbitrary since steady state), fixed = False, Q_gen = 50 W
    sys_sim.add_node(1, "Heater", c_to_k(20.0), 100.0, q_gen=50.0, is_fixed=False)
    # Ambient Node at 25 C
    sys_sim.add_node(2, "Ambient", c_to_k(25.0), 1.0, q_gen=0.0, is_fixed=True)
    
    # Conduction: G = 2.0 W/K
    sys_sim.add_link(101, 1, 2, 0, 2.0)
    
    # Steady state heat balance: Q_gen = G * (T_1 - T_2) => 50 = 2 * (T_1 - 25) => T_1 = 50 C
    expected_temp_c = 50.0
    
    iters = sys_sim.solve_steady_state(tolerance=1e-6, max_iterations=1000)
    sim_temp_c = k_to_c(sys_sim.get_node_temperature(1))
    
    print(f"Iterations: {iters}")
    print(f"Analytical Steady Temp: {expected_temp_c:.4f} C")
    print(f"Simulated Steady Temp: {sim_temp_c:.4f} C")
    
    diff = abs(sim_temp_c - expected_temp_c)
    print(f"Difference: {diff:.6f} C")
    
    assert diff < 1e-4, "Steady state solver matches analytical solution"
    print("Steady-state test passed successfully!")

def test_stiff_system_implicit():
    print("\nRunning stiff system stability test...")
    sys_sim = ThermalSystem()
    
    # Stiff system: Node 1 has very small capacity (0.01 J/K) and is connected
    # to Ambient (fixed 20 C) with high conduction link (100 W/K)
    # The time constant is tau = C / G = 0.01 / 100 = 0.0001 seconds!
    # If dt = 1.0s, explicit Euler will blow up immediately (unstable).
    # RK4 stability limit is also exceeded (explodes).
    # But Implicit Euler should be stable and converge to 20 C.
    sys_sim.add_node(1, "HotSmallMass", c_to_k(100.0), 0.01, q_gen=0.0, is_fixed=False)
    sys_sim.add_node(2, "Ambient", c_to_k(20.0), 1.0, q_gen=0.0, is_fixed=True)
    sys_sim.add_link(101, 1, 2, 0, 100.0)
    
    # Run 5 steps of 1.0 second each
    dt = 1.0
    for _ in range(5):
        sys_sim.step_backward_euler(dt, tolerance=1e-5, max_iterations=50)
        
    temp_implicit = k_to_c(sys_sim.get_node_temperature(1))
    print(f"Stiff Mass Temp after 5s of dt=1s (Implicit): {temp_implicit:.4f} C")
    
    # Verify it converges stably (close to ambient)
    assert abs(temp_implicit - 20.0) < 1e-2, "Implicit Euler converged stably for stiff system"
    print("Stiff system stability test passed successfully!")

def test_temperature_dependence():
    print("\nRunning temperature-dependent properties test...")
    sys_sim = ThermalSystem()
    
    # Hot mass with temp-dependent capacity: C(T) = c_a0 + c_a1 * T + c_a2 * T^2
    # C_a0 = 1000 J/K
    # Let's set c_a1 = 2.0 J/K^2 (linear coefficient)
    # Init temp = 100 C (373.15 K)
    # So initial capacity is 1000 + 2.0 * 373.15 = 1746.3 J/K
    # Ambient fixed at 20 C (293.15 K)
    # Conduction link: G(T_avg) = g_a0 + g_a1 * T_avg + g_a2 * T_avg^2
    # g_a0 = 10 W/K
    # Let's set g_a1 = 0.05 W/K^2
    # Init T_avg = 0.5 * (373.15 + 293.15) = 333.15 K
    # Init G = 10 + 0.05 * 333.15 = 26.6575 W/K
    
    sys_sim.add_node(1, "HotMass", c_to_k(100.0), 1000.0, q_gen=0.0, is_fixed=False, c_a1=2.0, c_a2=0.0)
    sys_sim.add_node(2, "Ambient", c_to_k(20.0), 1.0, q_gen=0.0, is_fixed=True)
    sys_sim.add_link(101, 1, 2, 0, 10.0, 0.0, g_a1=0.05, g_a2=0.0)
    
    # Run 1 step of dt = 2.0s using Implicit Euler
    dt = 2.0
    sys_sim.step_backward_euler(dt, tolerance=1e-6, max_iterations=100)
    
    temp_implicit = k_to_c(sys_sim.get_node_temperature(1))
    print(f"Node Temp after 2s (Implicit with T-dependence): {temp_implicit:.4f} C")
    
    # Let's run a comparable test with constant parameters (c_a1 = 0, g_a1 = 0)
    sys_const = ThermalSystem()
    sys_const.add_node(1, "HotMass", c_to_k(100.0), 1000.0, q_gen=0.0, is_fixed=False)
    sys_const.add_node(2, "Ambient", c_to_k(20.0), 1.0, q_gen=0.0, is_fixed=True)
    sys_const.add_link(101, 1, 2, 0, 10.0, 0.0)
    sys_const.step_backward_euler(dt, tolerance=1e-6, max_iterations=100)
    
    temp_const = k_to_c(sys_const.get_node_temperature(1))
    print(f"Node Temp after 2s (Constant properties): {temp_const:.4f} C")
    
    assert temp_implicit < temp_const, "Temperature-dependent simulation behaves differently as expected"
    print("Temperature-dependent properties test passed successfully!")

if __name__ == "__main__":
    try:
        test_transient_cooling()
        test_steady_state()
        test_stiff_system_implicit()
        test_temperature_dependence()
        print("\nAll tests passed successfully!")
    except AssertionError as ae:
        print(f"\nAssertion Error: {ae}")
        sys.exit(1)
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        sys.exit(1)
