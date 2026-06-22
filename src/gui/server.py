import http.server
import socketserver
import json
import os
import sys
import webbrowser

# Add wrapper to path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from wrapper.thermal_solver import ThermalSystem, c_to_k, k_to_c

PORT = 8000
sys_instance = None

class ThermalSimRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        # Set the directory for static file serving to src/gui/web
        base_dir = os.path.dirname(os.path.abspath(__file__))
        self.web_dir = os.path.join(base_dir, "web")
        super().__init__(*args, directory=self.web_dir, **kwargs)

    def do_POST(self):
        global sys_instance
        if sys_instance is None:
            sys_instance = ThermalSystem()

        if self.path == '/api/init':
            self.handle_init()
        elif self.path == '/api/step':
            self.handle_step()
        elif self.path == '/api/solve_steady':
            self.handle_solve_steady()
        else:
            self.send_error(404, "API endpoint not found")

    def handle_init(self):
        global sys_instance
        try:
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            data = json.loads(post_data.decode('utf-8'))

            sys_instance.clear()

            # Add nodes
            for node in data.get('nodes', []):
                dom_type = int(node.get('domain_type', node.get('domain', 0)))
                sys_instance.add_node(
                    node_id=int(node['id']),
                    name=node['name'],
                    init_temp_k=float(node['temp_k']),
                    capacity=float(node['capacity']),
                    q_gen=float(node.get('q_gen', 0.0)),
                    is_fixed=bool(node.get('is_fixed', False)),
                    c_a1=float(node.get('c_a1', 0.0)),
                    c_a2=float(node.get('c_a2', 0.0)),
                    domain_type=dom_type,
                    fluid_medium=node.get('fluid_medium', ''),
                    fluid_volume=float(node.get('fluid_volume', 0.0))
                )
                if dom_type == 1:
                    sys_instance.set_node_fluid_params(
                        int(node['id']),
                        float(node.get('fluid_mix_ratio', 0.5)),
                        float(node.get('fluid_rho_a0', 1000.0)),
                        float(node.get('fluid_rho_a1', 0.0)),
                        float(node.get('fluid_rho_a2', 0.0)),
                        float(node.get('fluid_cp_a0', 4184.0)),
                        float(node.get('fluid_cp_a1', 0.0)),
                        float(node.get('fluid_cp_a2', 0.0))
                    )

            # Add links
            for link in data.get('links', []):
                sys_instance.add_link(
                    link_id=int(link['id']),
                    node_a_id=int(link['node_a']),
                    node_b_id=int(link['node_b']),
                    link_type=int(link['type']),
                    p1=float(link['p1']),
                    p2=float(link.get('p2', 0.0)),
                    g_a1=float(link.get('g_a1', 0.0)),
                    g_a2=float(link.get('g_a2', 0.0)),
                    fan_area=float(link.get('fan_area', 0.005))
                )

            self.send_json_response({"status": "success", "message": "System initialized successfully"})
        except Exception as e:
            self.send_json_response({"status": "error", "message": str(e)}, status_code=400)

    def handle_step(self):
        global sys_instance
        try:
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            data = json.loads(post_data.decode('utf-8'))

            dt = float(data.get('dt', 0.1))
            solver_type = data.get('solver', 'rk4')
            
            # Apply dynamic updates to nodes (like changing q_gen or target temp on the fly)
            for node in data.get('nodes', []):
                dom_type = int(node.get('domain_type', node.get('domain', 0)))
                sys_instance.update_node(
                    node_id=int(node['id']),
                    temp_k=float(node['temp_k']),
                    capacity=float(node['capacity']),
                    q_gen=float(node['q_gen']),
                    is_fixed=bool(node['is_fixed']),
                    c_a1=float(node.get('c_a1', 0.0)),
                    c_a2=float(node.get('c_a2', 0.0)),
                    domain_type=dom_type,
                    fluid_medium=node.get('fluid_medium', ''),
                    fluid_volume=float(node.get('fluid_volume', 0.0))
                )
                if dom_type == 1:
                    sys_instance.set_node_fluid_params(
                        int(node['id']),
                        float(node.get('fluid_mix_ratio', 0.5)),
                        float(node.get('fluid_rho_a0', 1000.0)),
                        float(node.get('fluid_rho_a1', 0.0)),
                        float(node.get('fluid_rho_a2', 0.0)),
                        float(node.get('fluid_cp_a0', 4184.0)),
                        float(node.get('fluid_cp_a1', 0.0)),
                        float(node.get('fluid_cp_a2', 0.0))
                    )

            # Apply dynamic updates to links (like changing conductance or flow rate)
            for link in data.get('links', []):
                sys_instance.update_link(
                    link_id=int(link['id']),
                    p1=float(link['p1']),
                    p2=float(link.get('p2', 0.0)),
                    g_a1=float(link.get('g_a1', 0.0)),
                    g_a2=float(link.get('g_a2', 0.0)),
                    fan_area=float(link.get('fan_area', 0.005))
                )

            # Perform time step
            if solver_type == 'euler':
                sys_instance.step_explicit_euler(dt)
            else:
                sys_instance.step_rk4(dt)

            # Fetch new temperatures
            temps = sys_instance.get_nodes_data()
            self.send_json_response({"status": "success", "temperatures": temps})
        except Exception as e:
            self.send_json_response({"status": "error", "message": str(e)}, status_code=400)

    def handle_solve_steady(self):
        global sys_instance
        try:
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            data = json.loads(post_data.decode('utf-8'))

            tolerance = float(data.get('tolerance', 1e-4))
            max_iterations = int(data.get('max_iterations', 1000))

            # Apply dynamic updates
            for node in data.get('nodes', []):
                dom_type = int(node.get('domain_type', node.get('domain', 0)))
                sys_instance.update_node(
                    node_id=int(node['id']),
                    temp_k=float(node['temp_k']),
                    capacity=float(node['capacity']),
                    q_gen=float(node['q_gen']),
                    is_fixed=bool(node['is_fixed']),
                    c_a1=float(node.get('c_a1', 0.0)),
                    c_a2=float(node.get('c_a2', 0.0)),
                    domain_type=dom_type,
                    fluid_medium=node.get('fluid_medium', ''),
                    fluid_volume=float(node.get('fluid_volume', 0.0))
                )
                if dom_type == 1:
                    sys_instance.set_node_fluid_params(
                        int(node['id']),
                        float(node.get('fluid_mix_ratio', 0.5)),
                        float(node.get('fluid_rho_a0', 1000.0)),
                        float(node.get('fluid_rho_a1', 0.0)),
                        float(node.get('fluid_rho_a2', 0.0)),
                        float(node.get('fluid_cp_a0', 4184.0)),
                        float(node.get('fluid_cp_a1', 0.0)),
                        float(node.get('fluid_cp_a2', 0.0))
                    )

            for link in data.get('links', []):
                sys_instance.update_link(
                    link_id=int(link['id']),
                    p1=float(link['p1']),
                    p2=float(link.get('p2', 0.0)),
                    g_a1=float(link.get('g_a1', 0.0)),
                    g_a2=float(link.get('g_a2', 0.0)),
                    fan_area=float(link.get('fan_area', 0.005))
                )

            # Solve steady state
            iterations = sys_instance.solve_steady_state(tolerance, max_iterations)
            temps = sys_instance.get_nodes_data()
            
            self.send_json_response({
                "status": "success",
                "iterations": iterations,
                "temperatures": temps
            })
        except Exception as e:
            self.send_json_response({"status": "error", "message": str(e)}, status_code=400)

    def send_json_response(self, data, status_code=200):
        self.send_response(status_code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode('utf-8'))

    # Enable CORS for development convenience
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

def start_server():
    global sys_instance
    # Ensure system is initialized
    sys_instance = ThermalSystem()
    
    # Check if web directory exists
    base_dir = os.path.dirname(os.path.abspath(__file__))
    web_dir = os.path.join(base_dir, "web")
    if not os.path.exists(web_dir):
        os.makedirs(web_dir)
        
    class ThreadingTCPServer(socketserver.ThreadingTCPServer):
        allow_reuse_address = True

    with ThreadingTCPServer(("", PORT), ThermalSimRequestHandler) as httpd:
        print(f"Server started on http://localhost:{PORT}")
        webbrowser.open(f"http://localhost:{PORT}")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server...")
        finally:
            if sys_instance:
                sys_instance.destroy()

if __name__ == "__main__":
    start_server()
