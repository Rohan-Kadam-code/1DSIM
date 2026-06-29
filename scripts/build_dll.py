"""
build_dll.py — Compile the L1_physics solver into build/solver.dll
Run from project root: python scripts/build_dll.py
"""
import os
import subprocess
import sys

def build_cpp():
    print("Starting C++ solver DLL compilation...")

    # Path settings — all relative to project root
    physics_dir = os.path.join("src", "L1_physics")
    solver_cpp = os.path.join(physics_dir, "solver.cpp")
    bindings_cpp = os.path.join(physics_dir, "bindings.cpp")

    # Output to build/ directory
    os.makedirs("build", exist_ok=True)
    output_dll = os.path.join("build", "solver.dll")

    # Visual Studio 2022 Build Tools default path
    vcvars_path = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

    if not os.path.exists(vcvars_path):
        possible_paths = [
            r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
            r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
            r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
            r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
            r"C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
        ]
        for p in possible_paths:
            if os.path.exists(p):
                vcvars_path = p
                break

    if not os.path.exists(vcvars_path):
        print("Error: Could not find vcvars64.bat to set up MSVC environment.")
        print("Please verify your Visual Studio / Build Tools installation.")
        sys.exit(1)

    print(f"Using MSVC environment script: {vcvars_path}")

    # Construct compilation command
    # /LD: Create DLL
    # /EHsc: Enable C++ exceptions
    # /O2: Maximize speed
    # /Fe: Output file name
    cmd = f'call "{vcvars_path}" && cl /LD /EHsc /O2 /DNOMINMAX /Fe:"{output_dll}" "{solver_cpp}" "{bindings_cpp}"'

    print(f"Running compilation command:\n{cmd}\n")

    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        print("--- Compiler Stdout ---")
        print(result.stdout)

        if result.stderr:
            print("--- Compiler Stderr ---")
            print(result.stderr)

        if result.returncode == 0:
            print(f"C++ solver compiled successfully to {output_dll}!")
            # Clean up stray compiler artifacts from project root
            for f in ["solver.obj", "bindings.obj", "solver.lib", "solver.exp"]:
                if os.path.exists(f):
                    try:
                        os.remove(f)
                    except OSError:
                        pass
            print("Compilation artifacts cleaned.")
            return True
        else:
            print(f"Compilation failed with exit code: {result.returncode}")
            return False

    except Exception as e:
        print(f"Error during compilation process: {e}")
        return False

if __name__ == "__main__":
    success = build_cpp()
    sys.exit(0 if success else 1)
