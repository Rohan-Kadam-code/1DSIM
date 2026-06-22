import os
import sys
import subprocess

def main():
    print("====================================================")
    print("        1D Thermal Simulation Dashboard             ")
    print("====================================================")
    
    # Ensure C++ DLL is compiled
    dll_path = os.path.join("src", "core", "solver.dll")
    if not os.path.exists(dll_path):
        print("C++ Solver DLL not found. Compiling...")
        subprocess.run([sys.executable, "build.py"], check=True)
    
    server_path = os.path.join("src", "gui", "server.py")
    try:
        subprocess.run([sys.executable, server_path])
    except KeyboardInterrupt:
        print("\nExiting dashboard.")

if __name__ == "__main__":
    main()
