import os
import sys
from ctypes import CDLL, c_void_p, c_int, c_double, c_bool, c_char_p, POINTER, byref

class ThermalSolverError(Exception):
    pass

class ThermalSolverWrapper:
    def __init__(self):
        # Locate the DLL — project_root/build/solver.dll
        project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        dll_path = os.path.join(project_root, "build", "solver.dll")
        
        if not os.path.exists(dll_path):
            raise ThermalSolverError(f"C++ Solver DLL not found at: {dll_path}. Please build it first.")
            
        try:
            self.lib = CDLL(dll_path)
            self._setup_bindings()
        except Exception as e:
            raise ThermalSolverError(f"Failed to load C++ Solver DLL: {e}")
            
    def _setup_bindings(self):
        # Define argtypes and restypes for safety
        self.lib.create_system.argtypes = []
        self.lib.create_system.restype = c_void_p
        
        self.lib.destroy_system.argtypes = [c_void_p]
        self.lib.destroy_system.restype = None
        
        self.lib.add_node.argtypes = [c_void_p, c_int, c_char_p, c_double, c_double, c_double, c_bool, c_double, c_double, c_int, c_char_p, c_double]
        self.lib.add_node.restype = None
        
        self.lib.add_link.argtypes = [c_void_p, c_int, c_int, c_int, c_int, c_double, c_double, c_double, c_double, c_double]
        self.lib.add_link.restype = None
        
        self.lib.update_node.argtypes = [c_void_p, c_int, c_double, c_double, c_double, c_bool, c_double, c_double, c_int, c_char_p, c_double]
        self.lib.update_node.restype = None
        
        self.lib.update_link.argtypes = [c_void_p, c_int, c_double, c_double, c_double, c_double, c_double]
        self.lib.update_link.restype = None
        
        self.lib.clear_system.argtypes = [c_void_p]
        self.lib.clear_system.restype = None
        
        self.lib.step_explicit_euler.argtypes = [c_void_p, c_double]
        self.lib.step_explicit_euler.restype = None
        
        self.lib.step_rk4.argtypes = [c_void_p, c_double]
        self.lib.step_rk4.restype = None
        
        self.lib.step_backward_euler.argtypes = [c_void_p, c_double, c_double, c_int]
        self.lib.step_backward_euler.restype = None
        
        self.lib.solve_steady_state.argtypes = [c_void_p, c_double, c_int]
        self.lib.solve_steady_state.restype = c_int
        
        self.lib.get_node_temperature.argtypes = [c_void_p, c_int]
        self.lib.get_node_temperature.restype = c_double
        
        self.lib.get_node_count.argtypes = [c_void_p]
        self.lib.get_node_count.restype = c_int
        
        self.lib.get_nodes_data.argtypes = [c_void_p, POINTER(c_int), POINTER(c_double)]
        self.lib.get_nodes_data.restype = c_int

        self.lib.set_node_fluid_params.argtypes = [c_void_p, c_int, c_double, c_double, c_double, c_double, c_double, c_double, c_double]
        self.lib.set_node_fluid_params.restype = None

class ThermalSystem:
    def __init__(self):
        self.wrapper = ThermalSolverWrapper()
        self.sys_ptr = self.wrapper.lib.create_system()
        if not self.sys_ptr:
            raise ThermalSolverError("Failed to create ThermalSystem in C++ core.")
            
    def __del__(self):
        self.destroy()
        
    def destroy(self):
        if hasattr(self, 'sys_ptr') and self.sys_ptr:
            self.wrapper.lib.destroy_system(self.sys_ptr)
            self.sys_ptr = None
            
    def __enter__(self):
        return self
        
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.destroy()

    def add_node(self, node_id, name, init_temp_k, capacity, q_gen=0.0, is_fixed=False, c_a1=0.0, c_a2=0.0, domain_type=0, fluid_medium="", fluid_volume=0.0):
        """
        Adds a thermal node to the system.
        init_temp_k: temperature in Kelvin
        capacity: J/K
        q_gen: W
        is_fixed: bool boundary condition
        c_a1, c_a2: polynomial temperature coefficients
        domain_type: 0 (Solid), 1 (Fluid)
        fluid_medium: string, e.g. "Water"
        fluid_volume: volume in Liters
        """
        name_bytes = name.encode('utf-8')
        fluid_medium_bytes = fluid_medium.encode('utf-8')
        self.wrapper.lib.add_node(self.sys_ptr, node_id, name_bytes, init_temp_k, capacity, q_gen, is_fixed, c_a1, c_a2, domain_type, fluid_medium_bytes, fluid_volume)
        
    def add_link(self, link_id, node_a_id, node_b_id, link_type, p1, p2=0.0, g_a1=0.0, g_a2=0.0, fan_area=0.005):
        """
        Adds a thermal link connecting two nodes.
        link_type: 0 (Conduction), 1 (Convection), 2 (Radiation), 3 (Flow), 4 (Fan)
        p1: G for Conduction, hA for Convection, sigma*epsilon*A for Radiation, m_dot*cp for Flow, P_max for Fan
        p2: Direction for Flow (1.0 or -1.0), v_max for Fan
        g_a1, g_a2: polynomial temperature coefficients, or K and R system resistance coefficients for Fan
        fan_area: cross-sectional flow area (m^2)
        """
        self.wrapper.lib.add_link(self.sys_ptr, link_id, node_a_id, node_b_id, link_type, p1, p2, g_a1, g_a2, fan_area)
        
    def update_node(self, node_id, temp_k, capacity, q_gen, is_fixed, c_a1=0.0, c_a2=0.0, domain_type=0, fluid_medium="", fluid_volume=0.0):
        fluid_medium_bytes = fluid_medium.encode('utf-8')
        self.wrapper.lib.update_node(self.sys_ptr, node_id, temp_k, capacity, q_gen, is_fixed, c_a1, c_a2, domain_type, fluid_medium_bytes, fluid_volume)
        
    def set_node_fluid_params(self, node_id, mix_ratio, rho_a0, rho_a1, rho_a2, cp_a0, cp_a1, cp_a2):
        self.wrapper.lib.set_node_fluid_params(self.sys_ptr, node_id, mix_ratio, rho_a0, rho_a1, rho_a2, cp_a0, cp_a1, cp_a2)
        
    def update_link(self, link_id, p1, p2=0.0, g_a1=0.0, g_a2=0.0, fan_area=0.005):
        self.wrapper.lib.update_link(self.sys_ptr, link_id, p1, p2, g_a1, g_a2, fan_area)
        
    def clear(self):
        self.wrapper.lib.clear_system(self.sys_ptr)
        
    def step_explicit_euler(self, dt):
        self.wrapper.lib.step_explicit_euler(self.sys_ptr, dt)
        
    def step_rk4(self, dt):
        self.wrapper.lib.step_rk4(self.sys_ptr, dt)
        
    def step_backward_euler(self, dt, tolerance=1e-5, max_iterations=50):
        self.wrapper.lib.step_backward_euler(self.sys_ptr, dt, tolerance, max_iterations)
        
    def solve_steady_state(self, tolerance=1e-4, max_iterations=5000):
        return self.wrapper.lib.solve_steady_state(self.sys_ptr, tolerance, max_iterations)
        
    def get_node_temperature(self, node_id):
        return self.wrapper.lib.get_node_temperature(self.sys_ptr, node_id)
        
    def get_node_count(self):
        return self.wrapper.lib.get_node_count(self.sys_ptr)
        
    def get_nodes_data(self):
        """
        Returns a dict of {node_id: temperature_k} for all nodes in the system.
        """
        count = self.get_node_count()
        if count == 0:
            return {}
            
        ids_arr = (c_int * count)()
        temps_arr = (c_double * count)()
        
        actual_count = self.wrapper.lib.get_nodes_data(self.sys_ptr, ids_arr, temps_arr)
        
        data = {}
        for i in range(actual_count):
            data[ids_arr[i]] = temps_arr[i]
        return data

# Helpers for Celsius/Kelvin conversion
def k_to_c(temp_k):
    return temp_k - 273.15

def c_to_k(temp_c):
    return temp_c + 273.15
