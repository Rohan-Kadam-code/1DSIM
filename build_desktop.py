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
    
    # Define source files
    sources = [
        os.path.join("src", "core", "solver.cpp"),
        os.path.join("src", "desktop", "main.cpp"),
        os.path.join("thirdparty", "imgui", "imgui.cpp"),
        os.path.join("thirdparty", "imgui", "imgui_draw.cpp"),
        os.path.join("thirdparty", "imgui", "imgui_widgets.cpp"),
        os.path.join("thirdparty", "imgui", "imgui_tables.cpp"),
        os.path.join("thirdparty", "imgui", "backends", "imgui_impl_win32.cpp"),
        os.path.join("thirdparty", "imgui", "backends", "imgui_impl_dx11.cpp"),
        os.path.join("thirdparty", "implot", "implot.cpp"),
        os.path.join("thirdparty", "implot", "implot_items.cpp"),
    ]
    
    # Include directories
    includes = [
        "/Isrc/core",
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
    
    # Output file
    output_exe = "thermal_sim.exe"
    
    # Compilation command (C++17 standard)
    cmd = f'call "{vcvars_path}" && cl /EHsc /O2 /std:c++17 {includes_str} {sources_str} {libs_str} /Fe:"{output_exe}"'
    
    print("Running compilation command...")
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        print("--- Compiler Stdout ---")
        print(result.stdout)
        
        if result.stderr:
            print("--- Compiler Stderr ---")
            print(result.stderr)
            
        if result.returncode == 0:
            print("Native desktop application compiled successfully to thermal_sim.exe!")
            
            # Clean up compiler artifacts
            for root, dirs, files in os.walk("."):
                for f in files:
                    if f.endswith(".obj") and not root.startswith(os.path.join("thirdparty", "imgui")) and not root.startswith(os.path.join("thirdparty", "implot")):
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
