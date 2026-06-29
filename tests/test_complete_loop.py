import sys
import os
import math

# Add src to python path
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "src"))

from L4_sync.thermal_solver import ThermalSystem, c_to_k, k_to_c

def test_complete_loop_preset():
    print("Running complete cooling loop preset simulation test...")
    sys_sim = ThermalSystem()

    # Add all nodes in the loop
    # Engine (ID 100, Solid), Engine Jacket (ID 101, Fluid)
    sys_sim.add_node(100, "Engine Block", c_to_k(85.0), 55000.0, q_gen=25000.0, is_fixed=False)
    sys_sim.add_node(101, "Engine Jacket", c_to_k(80.0), 4000.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=4.5)

    # Hose 1 (ID 600, Fluid)
    sys_sim.add_node(600, "Hose: Engine to Tstat", c_to_k(80.0), 1000.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=0.5)

    # Thermostat (ID 500, Fluid)
    sys_sim.add_node(500, "Thermostat", c_to_k(80.0), 500.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=0.2)

    # Hose 2 (ID 610, Fluid)
    sys_sim.add_node(610, "Hose: Tstat to Radiator", c_to_k(80.0), 1000.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=0.5)

    # Radiator Coolant (ID 200, Fluid), Radiator Core (ID 201, Solid)
    sys_sim.add_node(200, "Radiator", c_to_k(70.0), 3000.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=3.0)
    sys_sim.add_node(201, "Radiator Core", c_to_k(60.0), 8000.0, q_gen=0.0, is_fixed=False)

    # Hose 3 (ID 620, Fluid)
    sys_sim.add_node(620, "Hose: Radiator to Pump", c_to_k(70.0), 1000.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=0.5)

    # Water Pump (ID 400, Fluid)
    sys_sim.add_node(400, "Water Pump", c_to_k(70.0), 1000.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=0.5)

    # Hose 4 (ID 630, Fluid)
    sys_sim.add_node(630, "Hose: Bypass to Pump", c_to_k(80.0), 1000.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=0.5)

    # Hose 5 (ID 640, Fluid)
    sys_sim.add_node(640, "Hose: Pump to Engine", c_to_k(70.0), 1000.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=0.5)

    # Expansion Tank (ID 700, Fluid)
    sys_sim.add_node(700, "Expansion Tank", c_to_k(70.0), 3000.0, q_gen=0.0, is_fixed=False, domain_type=1, fluid_medium="Water", fluid_volume=1.5)

    # Ambient Air (ID 300, Solid Boundary)
    sys_sim.add_node(300, "Ambient Air", c_to_k(25.0), 1.0, q_gen=0.0, is_fixed=True)


    # Add links
    # Internal Engine Block -> Jacket conduction
    sys_sim.add_link(102, 100, 101, 0, 1200.0) # Conduction

    # Engine Jacket -> Hose 1
    sys_sim.add_link(1001, 101, 600, 3, 35.0, 1.0) # Flow

    # Hose 1 -> Thermostat
    sys_sim.add_link(1002, 600, 500, 3, 35.0, 1.0) # Flow

    # Thermostat -> Hose 2 (Main)
    sys_sim.add_link(1003, 500, 610, 3, 17.5, 1.0) # Flow (thermostat partially open)

    # Hose 2 -> Radiator
    sys_sim.add_link(1004, 610, 200, 3, 17.5, 1.0) # Flow

    # Radiator -> Hose 3
    sys_sim.add_link(1005, 200, 620, 3, 17.5, 1.0) # Flow

    # Hose 3 -> Pump
    sys_sim.add_link(1006, 620, 400, 3, 17.5, 1.0) # Flow

    # Thermostat -> Hose 4 (Bypass)
    sys_sim.add_link(1007, 500, 630, 3, 17.5, 1.0) # Flow

    # Hose 4 -> Pump
    sys_sim.add_link(1008, 630, 400, 3, 17.5, 1.0) # Flow

    # Pump -> Hose 5
    sys_sim.add_link(1009, 400, 640, 3, 35.0, 1.0) # Flow

    # Hose 5 -> Engine
    sys_sim.add_link(1010, 640, 101, 3, 35.0, 1.0) # Flow

    # Internal Radiator coolant <-> core convection
    sys_sim.add_link(202, 200, 201, 1, 800.0) # Convection

    # Radiator core -> Ambient Air convection
    sys_sim.add_link(1011, 201, 300, 1, 600.0) # Convection

    # Expansion Tank conduction connection
    sys_sim.add_link(1012, 700, 620, 0, 0.5) # Conduction


    # Step the simulation
    dt = 0.05
    for _ in range(20):
        sys_sim.step_rk4(dt)

    t_engine = k_to_c(sys_sim.get_node_temperature(100))
    t_jacket = k_to_c(sys_sim.get_node_temperature(101))
    t_rad = k_to_c(sys_sim.get_node_temperature(200))

    print(f"Transient Test - Engine Temp: {t_engine:.2f} C, Jacket Temp: {t_jacket:.2f} C, Radiator Temp: {t_rad:.2f} C")

    # The temperatures should be reasonable and finite
    assert not math.isnan(t_engine)
    assert not math.isnan(t_jacket)
    assert not math.isnan(t_rad)
    assert t_engine > 20.0
    assert t_jacket > 20.0

    # Solve Steady-State
    iters = sys_sim.solve_steady_state(tolerance=1e-6, max_iterations=2000)
    t_engine_ss = k_to_c(sys_sim.get_node_temperature(100))
    t_jacket_ss = k_to_c(sys_sim.get_node_temperature(101))
    t_rad_ss = k_to_c(sys_sim.get_node_temperature(200))

    print(f"Steady-State Test ({iters} sweeps) - Engine: {t_engine_ss:.2f} C, Jacket: {t_jacket_ss:.2f} C, Radiator: {t_rad_ss:.2f} C")
    assert not math.isnan(t_engine_ss)
    assert t_engine_ss > 80.0

if __name__ == "__main__":
    test_complete_loop_preset()
