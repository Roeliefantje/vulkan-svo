import subprocess
import time
import re
from statistics import mean
from pathlib import Path


def run_and_analyze(program_path, working_dir):
    # Ensure the working directory exists
    working_dir = Path(working_dir)
    if not working_dir.exists():
        raise FileNotFoundError(f"Working directory not found: {working_dir}")

    # Compile regex patterns for performance
    fps_pattern = re.compile(r"FPS:\s*([\d.]+),\s*msPf:\s*([\d.]+)")
    steps_pattern = re.compile(r"Average Steps per ray:\s*([\d.]+)")

    fps_values = []
    mspf_values = []
    steps_values = []

    # Start the process
    process = subprocess.Popen(
        [program_path],
        cwd=working_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        bufsize=1,
    )

    print(f"Running {program_path} in {working_dir} for 60 seconds...\n")

    start_time = time.time()
    try:
        for line in process.stdout:
            line = line.strip()
            print(line)  # Optional: show live output
            # Parse FPS and msPf
            if match := fps_pattern.search(line):
                fps_values.append(float(match.group(1)))
                mspf_values.append(float(match.group(2)))
            # Parse average steps per ray
            elif match := steps_pattern.search(line):
                steps_values.append(float(match.group(1)))

            # Stop after 60 seconds
            if time.time() - start_time > 60:
                process.terminate()
                break

        # Wait a bit for graceful shutdown
        process.wait(timeout=5)

    except KeyboardInterrupt:
        process.terminate()
    except subprocess.TimeoutExpired:
        process.kill()

    # Compute averages
    avg_fps = mean(fps_values) if fps_values else 0
    avg_mspf = mean(mspf_values) if mspf_values else 0
    avg_steps = mean(steps_values) if steps_values else 0

    print("\n--- AVERAGE RESULTS ---")
    print(f"Average FPS: {avg_fps:.3f}")
    print(f"Average msPf: {avg_mspf:.3f}")
    print(f"Average Steps per ray: {avg_steps:.3f}")

    return avg_fps, avg_mspf, avg_steps


if __name__ == "__main__":
    # Example usage — replace these paths
    program = "../cmake-build-release-mingw/clion_vulkan.exe"  # or "./your_script.sh", etc.
    working_directory = "C:\\Users\\roeld\\Documents\\git\\Uni\\UU\\_Thesis\\clion-vulkan"

    run_and_analyze(program, working_directory)
