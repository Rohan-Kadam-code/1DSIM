"""
run.py — Entry point for the 1D Thermal Simulation Dashboard (web UI)
Run from project root: python run.py
"""
import os
import sys
import subprocess

def main():
    print("====================================================")
    print("        1D Thermal Simulation Dashboard             ")
    print("====================================================")

    # Ensure C++ DLL is compiled
    dll_path = os.path.join("build", "solver.dll")
    if not os.path.exists(dll_path):
        print("C++ Solver DLL not found. Compiling...")
        subprocess.run([sys.executable, os.path.join("scripts", "build_dll.py")], check=True)

    server_path = os.path.join("src", "L5_ui", "web", "server.py")
    try:
        subprocess.run([sys.executable, server_path])
    except KeyboardInterrupt:
        print("\nExiting dashboard.")

if __name__ == "__main__":
    main()
