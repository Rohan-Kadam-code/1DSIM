"""
build_desktop.py — Compile the full desktop ImGui application into build/thermal_sim.exe
Run from project root: python scripts/build_desktop.py
"""
import os
import subprocess
import sys

def clone_thirdparty():
    print("Setting up third-party libraries (Dear ImGui & ImPlot)...")
    os.makedirs("thirdparty", exist_ok=True)

    # Clone ImGui docking branch
    imgui_dir = os.path.join("thirdparty", "imgui")
    if not os.path.exists(imgui_dir):
        print("Cloning Dear ImGui (docking branch)...")
        try:
            subprocess.run([
                "git", "clone", "--depth", "1", "-b", "docking",
                "https://github.com/ocornut/imgui.git", imgui_dir
            ], check=True)
        except Exception as e:
            print(f"Error cloning ImGui: {e}")
            return False
    else:
        print("Dear ImGui already exists.")

    # Clone ImPlot
    implot_dir = os.path.join("thirdparty", "implot")
    if not os.path.exists(implot_dir):
        print("Cloning ImPlot...")
        try:
            subprocess.run([
                "git", "clone", "--depth", "1",
                "https://github.com/epezent/implot.git", implot_dir
            ], check=True)
        except Exception as e:
            print(f"Error cloning ImPlot: {e}")
            return False
    else:
        print("ImPlot already exists.")

    return True

def compile_desktop():
    print("Starting desktop C++ compilation...")

    # Path configuration
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
        print("Error: Could not find vcvars64.bat to configure compiler.")
        return False

    print(f"Using MSVC environment script: {vcvars_path}")

    # Output directory
    os.makedirs(os.path.join("build", "obj"), exist_ok=True)

    # Define source files — organized by layer
    sources = [
        # L1: Physics
        os.path.join("src", "L1_physics", "solver.cpp"),
        # L2: Domain
        os.path.join("src", "L2_domain", "component_library.cpp"),
        # L3: State
        os.path.join("src", "L3_state", "app_state.cpp"),
        os.path.join("src", "L3_state", "presets.cpp"),
        os.path.join("src", "L3_state", "serialization.cpp"),
        # L5: UI
        os.path.join("src", "L5_ui", "desktop", "main.cpp"),
        os.path.join("src", "L5_ui", "desktop", "ui_canvas.cpp"),
        os.path.join("src", "L5_ui", "desktop", "ui_panels.cpp"),
        # Thirdparty: ImGui
        os.path.join("thirdparty", "imgui", "imgui.cpp"),
        os.path.join("thirdparty", "imgui", "imgui_draw.cpp"),
        os.path.join("thirdparty", "imgui", "imgui_widgets.cpp"),
        os.path.join("thirdparty", "imgui", "imgui_tables.cpp"),
        os.path.join("thirdparty", "imgui", "backends", "imgui_impl_win32.cpp"),
        os.path.join("thirdparty", "imgui", "backends", "imgui_impl_dx11.cpp"),
        # Thirdparty: ImPlot
        os.path.join("thirdparty", "implot", "implot.cpp"),
        os.path.join("thirdparty", "implot", "implot_items.cpp"),
    ]

    # Include directories
    includes = [
        "/Isrc/L1_physics",
        "/Isrc/L2_domain",
        "/Isrc/L3_state",
        "/Isrc/L5_ui/desktop",
        "/Ithirdparty",
        "/Ithirdparty/imgui",
        "/Ithirdparty/imgui/backends",
        "/Ithirdparty/implot"
    ]

    # Standard libraries to link
    libs = [
        "d3d11.lib",
        "d3dcompiler.lib",
        "dxgi.lib",
        "user32.lib",
        "gdi32.lib",
        "shell32.lib",
        "ole32.lib",
        "comdlg32.lib"
    ]

    # Construct paths strings
    sources_str = " ".join([f'"{s}"' for s in sources])
    includes_str = " ".join(includes)
    libs_str = " ".join(libs)

    # Output to build/ directory
    output_exe = os.path.join("build", "thermal_sim.exe")
    obj_dir = os.path.join("build", "obj")

    # Compilation command (C++17 standard)
    # /Fo: Place .obj files in build/obj/
    cmd = f'call "{vcvars_path}" && cl /EHsc /O2 /std:c++17 /DNOMINMAX {includes_str} {sources_str} {libs_str} /Fo:"{obj_dir}\\\\" /Fe:"{output_exe}"'

    print("Running compilation command...")
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        print("--- Compiler Stdout ---")
        print(result.stdout)

        if result.stderr:
            print("--- Compiler Stderr ---")
            print(result.stderr)

        if result.returncode == 0:
            print(f"Desktop application compiled successfully to {output_exe}!")
            # Clean up any stray .obj in project root
            for root, dirs, files in os.walk("."):
                # Skip build/ and thirdparty/ directories
                if root.startswith(os.path.join(".", "build")) or root.startswith(os.path.join(".", "thirdparty")):
                    continue
                for f in files:
                    if f.endswith(".obj"):
                        try:
                            os.remove(os.path.join(root, f))
                        except OSError:
                            pass
            return True
        else:
            print(f"Compilation failed with exit code: {result.returncode}")
            return False
    except Exception as e:
        print(f"Error during compilation process: {e}")
        return False

if __name__ == "__main__":
    if clone_thirdparty():
        success = compile_desktop()
        sys.exit(0 if success else 1)
    else:
        sys.exit(1)
