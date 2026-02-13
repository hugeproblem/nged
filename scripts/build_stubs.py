import subprocess
import sys
import shutil
import os

def install_stubgen():
    try:
        import pybind11_stubgen
    except ImportError:
        print("Installing pybind11-stubgen...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "pybind11-stubgen"])

def build_stubs():
    print("Generating stubs for nged...")
    # Generate stubs into a temporary directory
    output_dir = "stubs"
    if os.path.exists(output_dir):
        shutil.rmtree(output_dir)
    
    # Run pybind11-stubgen
    # We target the 'nged' package.
    # Note: --ignore-invalid-expressions can help with the errors we saw
    cmd = [
        "pybind11-stubgen", 
        "nged", 
        "--output-dir", output_dir,
        "--ignore-all-errors", # suppress errors about unrepresentable defaults
        "--root-suffix", "" # cleaner output for packages
    ]
    
    try:
        subprocess.check_call(cmd)
    except subprocess.CalledProcessError as e:
        print(f"Error running pybind11-stubgen: {e}")
        return

    # Verify output
    stub_pkg = os.path.join(output_dir, "nged")
    if not os.path.exists(stub_pkg):
        print("Stub generation failed or produced unexpected output structure.")
        return

    print(f"Stubs generated in {output_dir}/nged")
    
    # Optional: copy to source tree if desired
    # source_pkg = "nged"
    # if os.path.exists(source_pkg):
    #    print(f"Copying stubs to source package {source_pkg}...")
    #    shutil.copytree(stub_pkg, source_pkg, dirs_exist_ok=True)

if __name__ == "__main__":
    install_stubgen()
    build_stubs()
