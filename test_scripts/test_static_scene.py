import csv
import subprocess
import time
import re
from statistics import mean
from pathlib import Path
import os
import pandas as pd
import matplotlib.pyplot as plt

CONFIGURATIONS = [
    # Chunkres, GridSize, GridHeight
    [512, 1, 5],
    [512, 3, 5],
    [512, 5, 5],
    [512, 7, 5],
    [512, 9, 5],
    [512, 11, 5],
    [512, 13, 5],
    [512, 15, 5],
    [512, 17, 5],
    [512, 19, 5],
    [512, 21, 5],
    [512, 23, 5],
    [512, 25, 5],
    [1024, 1, 5],
    [1024, 3, 5],
    [1024, 5, 5],
    [1024, 7, 5],
    [1024, 9, 5],
    [1024, 11, 5],
    [1024, 13, 5],
    [1024, 15, 5],
    [1024, 17, 5],
    [1024, 19, 5],
    [1024, 21, 5],
    [1024, 23, 5],
    [1024, 25, 5],
]


def run_and_analyze(program_path, working_dir, args):
    # Ensure the working directory exists
    working_dir = Path(working_dir)
    if not working_dir.exists():
        raise FileNotFoundError(f"Working directory not found: {working_dir}")

    # Generate the chunks needed for testing
    chunkgen = subprocess.run(
        [program_path, "--chunkgen", *args],
        cwd=working_dir
    )

    if chunkgen.returncode != 0:
        print(f"Error while generating the chunks!:\n{chunkgen.stderr}")

    # Compile regex patterns for performance
    fps_pattern = re.compile(r"FPS:\s*([\d.]+),\s*msPf:\s*([\d.]+)")
    steps_pattern = re.compile(r"Average Steps per ray:\s*([\d.]+)")

    fps_values = []
    mspf_values = []
    steps_values = []

    # Start the process
    process = subprocess.Popen(
        [program_path, *args],
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
            # print(line)  # Optional: show live output
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


def store_results(gridsize, gridheight, maxchunkresolution, fps, mspf, steps, filename="static_scene.csv"):
    # Ensure the 'results' directory exists
    os.makedirs("results", exist_ok=True)

    # Full path to the CSV file
    filepath = os.path.join("results", filename)

    # Check if the file exists (to know if we need to write the header)
    file_exists = os.path.isfile(filepath)

    # Open the CSV file for appending
    with open(filepath, mode='a', newline='') as csvfile:
        writer = csv.writer(csvfile)

        # Write header only if file is new
        if not file_exists:
            writer.writerow(["gridsize", "gridheight", "maxchunkresolution", "fps", "mspf", "steps"])

        # Write the data row
        writer.writerow([gridsize, gridheight, maxchunkresolution, fps, mspf, steps])

    print(f"Results saved to {filepath}")


def plot_combined_results(csv_path="results/static_scene.csv", output_dir="results/static_scene", location_nr=1):
    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # Read CSV
    df = pd.read_csv(csv_path)

    # Drop duplicates by configuration
    df = df.drop_duplicates(subset=["gridsize", "gridheight", "maxchunkresolution"], keep='first')

    # Compute total resolution
    df["total_resolution"] = df["maxchunkresolution"] * df["gridsize"] * 0.5

    # Group by maxchunkresolution
    grouped = df.groupby("maxchunkresolution")

    # Plot 1: Average MSPF vs total_resolution
    plt.figure()
    for maxchunk, data in grouped:
        avg_data = data.groupby("total_resolution", as_index=False).agg({"mspf": "mean"})
        plt.plot(avg_data["total_resolution"], avg_data["mspf"], marker='o', label=f"Chunk Resolution = {maxchunk}")
    plt.title(f"Average MSPF for location {location_nr}")
    plt.xlabel("Total Resolution (Max Chunk Resolution * Grid Size * 0.5)")
    plt.ylabel("Average MSPF")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, f"mspf_vs_total_resolution_location{location_nr}.png"))
    plt.close()

    # Plot 2: Steps vs total_resolution
    plt.figure()
    for maxchunk, data in grouped:
        avg_data = data.groupby("total_resolution", as_index=False).agg({"steps": "mean"})
        plt.plot(avg_data["total_resolution"], avg_data["steps"], marker='o', label=f"Chunk Resolution = {maxchunk}")
    plt.title(f"Average steps per ray at location {location_nr}")
    plt.xlabel("Total Resolution Max (Chunk Resolution * Grid Size * 0.5)")
    plt.ylabel("Average steps per ray")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, f"steps_vs_total_resolution_location{location_nr}.png"))
    plt.close()

    print(f"Combined graphs saved to '{output_dir}'")


def plot_results(csv_path="results/static_scene.csv", output_dir="results/static_scene"):
    # Ensure output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # Read CSV file
    df = pd.read_csv(csv_path)

    # Remove duplicates (keep first occurrence)
    df = df.drop_duplicates(subset=["gridsize", "gridheight", "maxchunkresolution"], keep='first')

    # Group by maxchunkresolution
    grouped = df.groupby("maxchunkresolution")

    for maxchunk, data in grouped:
        # Compute averages per gridsize
        avg_data = data.groupby("gridsize", as_index=False).agg({
            "mspf": "mean",
            "steps": "mean"
        })

        # Plot 1: Average mspf vs gridsize
        plt.figure()
        plt.plot(avg_data["gridsize"], avg_data["mspf"], marker='o')
        plt.title(f"Average ms per frame vs grid size (Chunk Resolution={maxchunk})")
        plt.xlabel("grid size")
        plt.ylabel("Average ms per frame")
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, f"mspf_vs_gridsize_{maxchunk}.png"))
        plt.close()

        # Plot 2: Steps vs gridsize
        plt.figure()
        plt.plot(avg_data["gridsize"], avg_data["steps"], marker='o')
        plt.title(f"Average ray steps vs Gridsize (Chunk Resolution={maxchunk})")
        plt.xlabel("grid size")
        plt.ylabel("average ray steps")
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, f"steps_vs_gridsize_{maxchunk}.png"))
        plt.close()

    print(f"Graphs saved to '{output_dir}'")


def load_existing_results(file_path):
    existing = set()
    if os.path.exists(file_path):
        with open(file_path, newline='') as f:
            reader = csv.DictReader(f)
            for row in reader:
                row_key = (int(row['gridsize']), int(row['gridheight']), int(row['maxchunkresolution']))
                existing.add(row_key)
    return existing


if __name__ == "__main__":
    # Example usage — replace these paths

    program = "../cmake-build-release-mingw/clion_vulkan.exe"  # or "./your_script.sh", etc.
    working_directory = "C:\\Users\\roeld\\Documents\\git\\Uni\\UU\\_Thesis\\clion-vulkan"

    for test in [0, 1]:
        results_file = f"static_scene_{test}.csv"
        existing_results = load_existing_results(f"results/{results_file}")
        for config in CONFIGURATIONS:
            grid_size, grid_height, resolution = config[1], config[2], config[0]
            key = (grid_size, grid_height, resolution)

            if key in existing_results:
                print(f"Skipping existing config: Grid={grid_size}, Height={grid_height}, Res={resolution}")
                continue  # ✅ skip this one

            print(f"Running new config:\n"
                  f"Max Chunk resolution: {resolution}\nGrid Size: {grid_size}\nGrid Height: {grid_height}\n")

            args = ["--test", str(test), "--res", str(resolution),
                    "--grid", str(grid_size), "--gridheight", str(grid_height)]

            fps, mspf, steps = run_and_analyze(program, working_directory, args)
            store_results(grid_size, grid_height, resolution, fps, mspf, steps, filename=results_file)

        plot_combined_results(csv_path=f"results/{results_file}", location_nr=test + 1)
